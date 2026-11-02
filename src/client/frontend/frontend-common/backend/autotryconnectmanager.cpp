#include "autotryconnectmanager.h"

#include <QAbstractItemModel>
#include <QMetaType>

#include "backend.h"
#include "locations/locationsmodel_roles.h"
#include "types/bruteforcesequence.h"
#include "types/connectionsettings.h"
#include "utils/log/categories.h"
#include "utils/ws_assert.h"

namespace {

// Errors that make every remaining attempt pointless: the account is refused or the machine cannot
// dial any endpoint, so walking the rest of the grid would only repeat them. On these the sweep
// stops and leaves the ordinary disconnect handler to report the specific error, rather than
// covering it with the generic Auto-Try failure. Everything else counts as this attempt failing.
bool isTerminalError(ConnectError err)
{
    switch (err) {
        case ConnectError::kAccountBlocked:
        case ConnectError::kLocalDnsServerNotAvailable:
        case ConnectError::kDnsServiceStartFailure:
        case ConnectError::kAdapterNotInstalled:
            return true;
        default:
            return false;
    }
}

} // namespace

AutoTryConnectManager::AutoTryConnectManager(Backend *backend, QObject *parent)
    : QObject(parent),
      backend_(backend)
{
    WS_ASSERT(backend_);
    connect(backend_, &Backend::connectStateChanged, this, &AutoTryConnectManager::onConnectStateChanged);
    attemptDelayTimer_.setSingleShot(true);
    connect(&attemptDelayTimer_, &QTimer::timeout, this, &AutoTryConnectManager::onAttemptDelayElapsed);
}

void AutoTryConnectManager::start(const LocationID &preferredLocation)
{
    if (isActive()) {
        qCDebug(LOG_BASIC) << "Auto-Try is already running, ignoring the request";
        return;
    }

    preferredLocation_ = preferredLocation;
    pendingStart_ = true;
    qCInfo(LOG_BASIC) << "Auto-Try requested for location" << preferredLocation.getHashString();

    if (backend_->isDisconnected()) {
        begin();
    } else {
        // A connection is up: drop it, then begin from the DISCONNECTED that follows.
        qCInfo(LOG_BASIC) << "Auto-Try disconnecting the active session before it starts";
        backend_->sendDisconnect();
    }
}

void AutoTryConnectManager::cancel()
{
    if (!isActive()) {
        return;
    }

    qCInfo(LOG_BASIC) << "Auto-Try cancelled";
    const bool wasConnecting = isRunning_ && !backend_->isDisconnected();
    finish();

    if (wasConnecting) {
        backend_->sendDisconnect();
    }
    emit cancelled();
}

void AutoTryConnectManager::begin()
{
    isRunning_ = true;
    pendingStart_ = false;

    plan_ = buildPlan(preferredLocation_);
    index_ = 0;
    if (plan_.isEmpty()) {
        qCInfo(LOG_BASIC) << "Auto-Try has nothing to try";
        finishFailed();
        return;
    }
    qCInfo(LOG_BASIC) << "Auto-Try starting:" << plan_.size() << "attempts";
    tryNext();
}

void AutoTryConnectManager::tryNext()
{
    if (!isRunning_) {
        return;
    }
    if (index_ >= plan_.size()) {
        qCInfo(LOG_BASIC) << "Auto-Try exhausted every protocol and location";
        finishFailed();
        return;
    }

    const Attempt &attempt = plan_[index_];
    ++index_;
    emit progressChanged(index_, plan_.size(), attempt.protocol.toLongString(), attempt.locationLabel);
    qCInfo(LOG_BASIC) << "Auto-Try attempt" << index_ << "/" << plan_.size() << attempt.protocol.toLongString() << attempt.locationLabel;

    // Manual mode: the sweep picks the location and the protocol, one endpoint at a time, so the
    // engine must not walk its own protocol list under us.
    sweepIssuedConnect_ = true;
    backend_->sendConnect(attempt.location,
                          types::ConnectionSettings(attempt.protocol, portForProtocol(attempt.protocol), false));
}

void AutoTryConnectManager::onConnectStateChanged(const types::ConnectState &connectState)
{
    if (connectState.connectState == CONNECT_STATE_CONNECTING) {
        // A CONNECTING we did not ask for is the user pressing the main button mid-sweep: hand the
        // session over and stop walking, rather than fighting them for it.
        if (isActive() && !sweepIssuedConnect_) {
            qCInfo(LOG_BASIC) << "Auto-Try stopping: the user took over the connection";
            finish();
            emit cancelled();
        }
        return;
    }

    if (connectState.connectState == CONNECT_STATE_CONNECTED) {
        if (isRunning_) {
            qCInfo(LOG_BASIC) << "Auto-Try connected";
            finishConnected();
        }
        return;
    }

    if (connectState.connectState != CONNECT_STATE_DISCONNECTED) {
        return;
    }

    if (pendingStart_) {
        begin();
        return;
    }
    if (!isRunning_) {
        return;
    }
    // The attempt is over. Until the next one issues its connect, any CONNECTING is the user's.
    sweepIssuedConnect_ = false;

    // The user ended the session from the main button mid-sweep.
    if (connectState.disconnectReason == DISCONNECTED_BY_USER) {
        qCInfo(LOG_BASIC) << "Auto-Try stopping: the user ended the session";
        finishCancelled();
        return;
    }

    // Consent revoked, the account is out of data, or the machine cannot dial any endpoint: no other
    // location will fare better. The app reports these itself, so the sweep only stops.
    if (connectState.disconnectReason == DISCONNECTED_BY_DATA_LIMIT ||
        (connectState.disconnectReason == DISCONNECTED_WITH_ERROR && isTerminalError(connectState.connectError))) {
        qCInfo(LOG_BASIC) << "Auto-Try stopping early on" << connectState.disconnectReason << static_cast<int>(connectState.connectError);
        finishAborted();
        return;
    }

    // This attempt failed. Let the engine settle before the next connect.
    attemptDelayTimer_.start(kAttemptDelayMs);
}

void AutoTryConnectManager::onAttemptDelayElapsed()
{
    tryNext();
}

void AutoTryConnectManager::finishConnected()
{
    finish();
    emit connected();
}

void AutoTryConnectManager::finishFailed()
{
    finish();
    emit failed();
}

void AutoTryConnectManager::finishCancelled()
{
    finish();
    emit cancelled();
}

void AutoTryConnectManager::finishAborted()
{
    finish();
    emit aborted();
}

void AutoTryConnectManager::finish()
{
    isRunning_ = false;
    pendingStart_ = false;
    sweepIssuedConnect_ = false;
    attemptDelayTimer_.stop();
}

QList<types::Protocol> AutoTryConnectManager::orderedProtocols() const
{
    const QVector<types::Protocol> available = backend_->getPreferencesHelper()->getAvailableProtocols();
    return types::BruteForceSequence::protocolOrder(backend_->getPreferencesHelper()->currentProtocol(),
                                                    QList<types::Protocol>(available.begin(), available.end()));
}

QVector<AutoTryConnectManager::Attempt> AutoTryConnectManager::buildPlan(const LocationID &preferredLocation) const
{
    const QList<types::Protocol> protocols = orderedProtocols();
    if (protocols.isEmpty()) {
        return {};
    }

    QAbstractItemModel *model = backend_->locationsModelManager()->locationsModel();
    QVector<LocationID> locations;
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex locationIndex = model->index(row, 0);
        for (int cityRow = 0; cityRow < model->rowCount(locationIndex); ++cityRow) {
            const QModelIndex cityIndex = model->index(cityRow, 0, locationIndex);
            const LocationID lid = cityIndex.data(gui_locations::kLocationId).value<LocationID>();
            // Only API cities form a grid to walk; a custom config or a static IP has one endpoint.
            if (lid.isCustomConfigsLocation() || lid.isStaticIpsLocation() || lid.isBestLocation()) {
                continue;
            }
            if (cityIndex.data(gui_locations::kIsDisabled).toBool() ||
                cityIndex.data(gui_locations::kIsShowAsPremium).toBool()) {
                continue;
            }
            locations << lid;
        }
    }

    // The location the user is on leads, so a working city is found on the protocol they already chose.
    if (preferredLocation.isValid() && locations.removeOne(preferredLocation)) {
        locations.prepend(preferredLocation);
    }

    QVector<Attempt> plan;
    for (const auto &pair : types::BruteForceSequence::sweep(protocols, locations)) {
        plan << Attempt{pair.first, pair.second, locationLabel(pair.second)};
    }
    return plan;
}

QString AutoTryConnectManager::locationLabel(const LocationID &id) const
{
    const QModelIndex index = backend_->locationsModelManager()->getIndexByLocationId(id);
    if (!index.isValid()) {
        return id.city();
    }

    const QString name = index.data(gui_locations::kName).toString();
    const QString nick = index.data(gui_locations::kDisplayNickname).toString();
    if (nick.isEmpty()) {
        return name;
    }
    return QString("%1, %2").arg(name, nick);
}

uint AutoTryConnectManager::portForProtocol(const types::Protocol &protocol) const
{
    const QVector<uint> ports = backend_->getPreferencesHelper()->getAvailablePortsForProtocol(protocol);
    if (!ports.isEmpty()) {
        return ports.first();
    }
    return types::Protocol::defaultPortForProtocol(protocol);
}
