#include "bruteforcesequence.test.h"

#include <QtTest>

#include "types/bruteforcesequence.h"

using types::BruteForceSequence;

using types::Protocol;

void TestBruteForceSequence::testCurrentProtocolIsTriedFirst()
{
    const QList<Protocol> available{Protocol::IKEV2, Protocol::OPENVPN_UDP, Protocol::OPENVPN_TCP, Protocol::WIREGUARD};

    const QList<Protocol> order = BruteForceSequence::protocolOrder(Protocol::OPENVPN_TCP, available);

    QCOMPARE(order, QList<Protocol>({Protocol::OPENVPN_TCP, Protocol::IKEV2, Protocol::OPENVPN_UDP, Protocol::WIREGUARD}));
}

void TestBruteForceSequence::testUnknownCurrentProtocolLeavesOrderUntouched()
{
    const QList<Protocol> available{Protocol::IKEV2, Protocol::OPENVPN_UDP, Protocol::OPENVPN_TCP};

    QCOMPARE(BruteForceSequence::protocolOrder(Protocol(), available), available);
    QCOMPARE(BruteForceSequence::protocolOrder(Protocol(Protocol::UNINITIALIZED), available), available);
    QCOMPARE(BruteForceSequence::protocolOrder(Protocol::WSTUNNEL, available), available);
}

void TestBruteForceSequence::testSweepIsProtocolMajor()
{
    const QList<Protocol> protocols{Protocol::IKEV2, Protocol::OPENVPN_UDP};
    const QVector<LocationID> locations{
        LocationID::createApiLocationId(10, "Toronto", "The Six"),
        LocationID::createApiLocationId(20, "New York", "The Big Apple"),
        LocationID::createApiLocationId(30, "London", "The City"),
    };

    const auto sweep = BruteForceSequence::sweep(protocols, locations);

    QCOMPARE(sweep.size(), 6);
    QCOMPARE(sweep[0].first, Protocol::IKEV2);
    QCOMPARE(sweep[0].second, locations[0]);
    QCOMPARE(sweep[1].first, Protocol::IKEV2);
    QCOMPARE(sweep[1].second, locations[1]);
    QCOMPARE(sweep[2].first, Protocol::IKEV2);
    QCOMPARE(sweep[2].second, locations[2]);
    QCOMPARE(sweep[3].first, Protocol::OPENVPN_UDP);
    QCOMPARE(sweep[3].second, locations[0]);
    QCOMPARE(sweep[4].first, Protocol::OPENVPN_UDP);
    QCOMPARE(sweep[4].second, locations[1]);
    QCOMPARE(sweep[5].first, Protocol::OPENVPN_UDP);
    QCOMPARE(sweep[5].second, locations[2]);
}

void TestBruteForceSequence::testEmptyInputsProduceNoAttempts()
{
    QVERIFY(BruteForceSequence::sweep({}, {LocationID::createApiLocationId(1, "Toronto", "The Six")}).isEmpty());
    QVERIFY(BruteForceSequence::sweep({Protocol::IKEV2}, {}).isEmpty());
}

QTEST_MAIN(TestBruteForceSequence)
