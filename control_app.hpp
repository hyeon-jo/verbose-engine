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
#include <boost/asio.hpp>
#include "messages.hpp"

enum { header_length = 8 };

enum MessageType : uint8_t {
    start = 19,
    stop = 24,
    configInfo = 26,
};

struct Backend {
    std::string host;
    std::array<int, 2> ports;
    std::string name;
    bool ready;
    std::array<std::shared_ptr<boost::asio::ip::tcp::socket>, 2> sockets;  // Store socket objects
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
    bool sendTcpMessage(uint8_t messageType, Backend& backend);
    bool setMessage(stDataRecordConfigMsg& msg, uint8_t messageType);
    void cleanupSockets();

    std::vector<Backend> backends;
    std::vector<QLineEdit*> ipInputs;
    std::vector<QLabel*> statusLabels;
    QPushButton* toggleBtn;
    QPushButton* eventBtn;
    QPushButton* applyBtn;
    QTimer* timer;
    QTimer* statusTimer;
    std::shared_ptr<boost::asio::io_context> io_context;

    bool isToggleOn;
    bool eventSent;
    uint32_t messageCounter;
}; 