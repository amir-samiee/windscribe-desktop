#pragma once

#include <QList>
#include <QPair>
#include <QVector>

#include "locationid.h"
#include "protocol.h"

namespace types {

// Ordering for the Auto-Try sweep: every location on one protocol before the next protocol, with the
// protocol already selected tried first. Pure, so the walk is testable without a device.
class BruteForceSequence
{
public:
    // `current` is the protocol the user is on, tried first; `available` is every protocol the app
    // supports, in the order the app lists them. An unset or unknown `current` leaves the order as is.
    static QList<Protocol> protocolOrder(const Protocol &current, const QList<Protocol> &available);

    // The full sweep as (protocol, location) pairs, protocol-major.
    static QVector<QPair<Protocol, LocationID>> sweep(const QList<Protocol> &protocols,
                                                      const QVector<LocationID> &locations);
};

} // namespace types
