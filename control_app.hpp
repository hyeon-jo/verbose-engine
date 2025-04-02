#pragma once

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtCore/QTimer>
#include <QtGui/QCloseEvent>
#include <vector>
#include <array>
#include <string>
#include <memory>

struct Backend {
    std::string host;
    std::array<int, 2> ports;
    std::string name;
    bool ready;
    std::array<int, 2> sockets;  // File descriptors for sockets
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

private:
    void setupUI();
    void centerWindow();
    bool sendTcpMessage(bool start = true);
    void cleanupSockets();

    std::vector<Backend> backends;
    std::vector<QLineEdit*> ipInputs;
    std::vector<QLabel*> statusLabels;
    QPushButton* toggleBtn;
    QPushButton* eventBtn;
    QPushButton* applyBtn;
    QTimer* timer;
    QTimer* statusTimer;

    bool isToggleOn;
    bool eventSent;
    uint32_t messageCounter;
}; 