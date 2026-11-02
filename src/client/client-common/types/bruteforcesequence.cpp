#include "bruteforcesequence.h"

namespace types {

QList<Protocol> BruteForceSequence::protocolOrder(const Protocol &current, const QList<Protocol> &available)
{
    if (!current.isValid() || !available.contains(current)) {
        return available;
    }

    QList<Protocol> order;
    order << current;
    for (const Protocol &p : available) {
        if (p != current) {
            order << p;
        }
    }
    return order;
}

QVector<QPair<Protocol, LocationID>> BruteForceSequence::sweep(const QList<Protocol> &protocols,
                                                              const QVector<LocationID> &locations)
{
    QVector<QPair<Protocol, LocationID>> plan;
    plan.reserve(protocols.size() * locations.size());
    for (const Protocol &protocol : protocols) {
        for (const LocationID &location : locations) {
            plan << qMakePair(protocol, location);
        }
    }
    return plan;
}

} // namespace types
