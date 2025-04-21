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
#include "image_viewer.hpp"


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

    // TCP Protocol functions
    void cleanupSockets();
    Header setHeader(uint8_t messageType);
    void writeHeader(Backend& backend, MessageType msgType);
    void parseHeader(char* headerBuffer, Header& header);
    Protocol_Header getReceivedHeader(Backend& backend, int idx);
    bool setDataRequestMessage(stDataRequestMsg& msg, uint8_t messageType);
    bool sendDataRequestMessage(Backend& backend, int idx);

    // Data logging control functions
    bool setRecordConfigMessage(stDataRecordConfigMsg& msg, uint8_t messageType);
    bool sendLoggingMessage(uint8_t messageType, Backend& backend, int idx);

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
    ImageViewer* imageViewer;

    bool isToggleOn;
    bool eventSent;
    uint32_t messageCounter;
}; 