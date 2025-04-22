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
#include <thread>
#include <atomic>
#include <boost/asio.hpp>
#include "messages.hpp"
#include "image_viewer.hpp"

class TcpClient;

struct Backend {
    std::string host;
    std::array<uint16_t, 2> ports;
    std::string name;
    bool ready;
    std::array<std::shared_ptr<boost::asio::ip::tcp::socket>, 2> sockets;
};

class ControlApp : public QMainWindow {
    Q_OBJECT

public:
    ControlApp(QWidget* parent = nullptr);
    ~ControlApp();
    void processData(const char* imageData, int sensorType, int width, int height);

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
    ImageViewer* imageViewer;
    TcpClient* tcpClient;

    bool isToggleOn;
    bool eventSent;
    uint32_t messageCounter;
    bool serverConnected;

    std::thread dataThread;
    std::thread receiveThread;
    std::atomic<bool> stopDataThread{false};
    std::atomic<bool> stopReceiveThread{false};
}; 