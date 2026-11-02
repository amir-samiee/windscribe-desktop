#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

#include "types/connectstate.h"
#include "types/locationid.h"
#include "types/protocol.h"

class Backend;

// Tries every available location on the current protocol, then every location on the next, until one
// connects or the whole grid has been tried. The desktop counterpart of the Android brute-force
// sweep: protocol-major (all locations first, so a working location is found on the protocol the user
// is already on) and driven by the connect-state stream rather than by a blocking loop.
//
// Only the built-in server list is swept: a custom config or a static IP authenticates against its
// own endpoint, so there is no grid of locations to walk.
class AutoTryConnectManager : public QObject
{
    Q_OBJECT
public:
    explicit AutoTryConnectManager(Backend *backend, QObject *parent = nullptr);

    // Starts a sweep, or ignores the call while one is already under way. A sweep requested while a
    // connection is up first drops it, then begins once the engine reports disconnected.
    void start(const LocationID &preferredLocation);

    // Stops a sweep and drops the connection it was working on.
    void cancel();

    // True from the moment a sweep is requested until it ends, including the teardown of a
    // connection the request had to drop first.
    bool isActive() const { return isRunning_ || pendingStart_; }

    // Latest step of a running sweep, for a UI that attaches late.
    int currentIndex() const { return index_; }
    int totalAttempts() const { return plan_.size(); }

signals:
    // Progress of a running sweep, for the UI. `index` is one-based.
    void progressChanged(int index, int total, const QString &protocolLabel, const QString &locationLabel);
    // Exactly one of these ends every sweep.
    void connected();
    // The whole grid was walked without a connection. The UI reports this one.
    void failed();
    // The user ended the session from the main button mid-sweep. Nothing to report.
    void cancelled();
    // Stopped early on an error no other location would get past. The ordinary disconnect handling
    // reports that error, so the UI shows nothing of its own.
    void aborted();

private slots:
    void onConnectStateChanged(const types::ConnectState &connectState);
    void onAttemptDelayElapsed();

private:
    struct Attempt
    {
        types::Protocol protocol;
        LocationID location;
        QString locationLabel;
    };

    void begin();
    void tryNext();
    void finish();
    void finishConnected();
    void finishFailed();
    void finishCancelled();
    void finishAborted();
    QList<types::Protocol> orderedProtocols() const;
    QVector<Attempt> buildPlan(const LocationID &preferredLocation) const;
    QString locationLabel(const LocationID &id) const;
    uint portForProtocol(const types::Protocol &protocol) const;

    // Between attempts, so the engine has settled out of the failed attempt before the next connect.
    static constexpr int kAttemptDelayMs = 250;

    Backend *backend_;
    bool isRunning_ = false;
    bool pendingStart_ = false;
    // True between a sweep attempt issuing its connect and that attempt ending. A CONNECTING while
    // this is false is the user taking over from the main button, not one of ours.
    bool sweepIssuedConnect_ = false;
    LocationID preferredLocation_;
    QVector<Attempt> plan_;
    int index_ = 0;
    QTimer attemptDelayTimer_;
};
