#pragma once

#include <QObject>
#include <QTest>

class TestBruteForceSequence : public QObject
{
    Q_OBJECT

private slots:
    void testCurrentProtocolIsTriedFirst();
    void testUnknownCurrentProtocolLeavesOrderUntouched();
    void testSweepIsProtocolMajor();
    void testEmptyInputsProduceNoAttempts();
};
