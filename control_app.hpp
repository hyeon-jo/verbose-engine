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
    bool sendLoggingMessage(uint8_t messageType, Backend& backend, int idx);
    Header setHeader(uint8_t messageType);
    bool setRecordConfigMessage(stDataRecordConfigMsg& msg, uint8_t messageType);
    bool setTCPMessage(stDataRequestMsg& msg, uint8_t messageType);
    bool getSensorDataFromServer(Backend& backend, int idx);
    void cleanupSockets();
    bool sendLinkMessage(Backend& backend, int idx);

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
    std::shared_ptr<boost::asio::io_context> io_context;

    bool isToggleOn;
    bool eventSent;
    uint32_t messageCounter;
}; 