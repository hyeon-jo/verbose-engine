#pragma once

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtCore/QTimer>
#include <QtCore/QThread>
#include <QtCore/QMutex>
#include <QtGui/QCloseEvent>
#include <vector>
#include <memory>
#include "backend_types.hpp"
#include "tcp_client.hpp"
#include "sensor_window.hpp"

class ConnectionThread : public QThread {
    Q_OBJECT

public:
    explicit ConnectionThread(TcpClient* client, std::vector<Backend>& backends, QObject* parent = nullptr);
    void stop();
    void startConnection();

protected:
    void run() override;

private:
    TcpClient* tcpClient;
    std::vector<Backend>& backends;
    QMutex mutex;
    bool stopRequested;
    bool shouldConnect;
};

class ControlApp : public QMainWindow {
    Q_OBJECT

public:
    ControlApp(QWidget* parent = nullptr);
    ~ControlApp();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void connectToServer();
    void applyConfiguration();
    void toggleAction();
    void sendEvent();
    void enableEventButton();
    void onConnectionStatusChanged(const Backend& backend, bool connected);
    void onMessageStatusChanged(const Backend& backend, bool success);

private:
    void setupUI();
    void centerWindow();
    void updateStatusLabel(const Backend& backend, bool connected);
    void cleanupConnections();
    void checkAllConnected();

    std::vector<Backend> backends;
    std::vector<QLineEdit*> ipInputs;
    std::vector<QLineEdit*> portInputs1;
    std::vector<QLineEdit*> portInputs2;
    std::vector<QLabel*> statusLabels;
    QPushButton* toggleBtn;
    QPushButton* eventBtn;
    QPushButton* applyBtn;
    QTimer* timer;
    QTimer* statusTimer;

    std::unique_ptr<TcpClient> tcpClient;
    std::unique_ptr<SensorWindow> sensorWindow;
    bool isToggleOn;
    bool eventSent;

    // 스레드 관련 멤버 변수
    ConnectionThread* connectionThread;
}; 