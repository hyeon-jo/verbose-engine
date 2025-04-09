#pragma once

#include <QTest>
#include <QObject>
#include <QSignalSpy>
#include "../sensor_window.hpp"

class TestSensorWindow : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testInitialization();
    void testSensorDataUpdate();
    void testConnectionStatus();
    void testErrorHandling();
    void testUIElements();
    void testDataValidation();
    void testPerformance();
}; 