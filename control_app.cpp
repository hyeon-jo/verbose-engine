#include "control_app.hpp"
#include "tcp_client.hpp"
#include <QApplication>
#include <QDesktopWidget>
#include <QMessageBox>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <thread>
#include <atomic>

ControlApp::ControlApp(QWidget* parent) : QMainWindow(parent), 
    isToggleOn(false), eventSent(false), messageCounter(0), serverConnected(false) {
    
    tcpClient = new TcpClient(this);
    setupUI();

    // Setup timers
    timer = new QTimer(this);
    QObject::connect(timer, &QTimer::timeout, this, &ControlApp::enableEventButton);

    statusTimer = new QTimer(this);
    QObject::connect(statusTimer, &QTimer::timeout, this, &ControlApp::connectToServer);
    statusTimer->start(1000);  // Check every 1 second

    // Initialize and show image viewer in a separate window
    imageViewer = new ImageViewer(nullptr);
    imageViewer->setWindowFlags(Qt::Window);
    imageViewer->show();
}

ControlApp::~ControlApp() {
    stopDataThread = true;
    stopReceiveThread = true;
    if (dataThread.joinable()) {
        dataThread.join();
    }
    if (receiveThread.joinable()) {
        receiveThread.join();
    }
    delete tcpClient;
}

void ControlApp::setupUI() {
    // Create central widget and layout
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(20);

    // Create IP configuration group
    QGroupBox* configGroup = new QGroupBox("Backend Configuration", this);
    QGridLayout* configLayout = new QGridLayout;
    configLayout->setSpacing(10);

    // Create input fields for each backend
    auto& backends = tcpClient->getBackends();
    for (size_t i = 0; i < backends.size(); ++i) {
        QLabel* ipLabel = new QLabel(QString::fromStdString(backends[i].name + " IP:"), this);
        ipLabel->setStyleSheet("font-size: 32px;");
        
        QLineEdit* ipInput = new QLineEdit(QString::fromStdString(backends[i].host), this);
        ipInput->setPlaceholderText("Enter IP address");
        ipInput->setStyleSheet(
            "QLineEdit {"
            "    font-size: 32px;"
            "    padding: 5px;"
            "    border: 1px solid #999;"
            "    border-radius: 3px;"
            "}"
            "QLineEdit:focus {"
            "    border: 2px solid #008CBA;"
            "}"
        );
        
        QLabel* portLabel1 = new QLabel("Port 1:", this);
        portLabel1->setStyleSheet("font-size: 32px;");
        
        QLineEdit* portInput1 = new QLineEdit(QString::number(backends[i].ports[0]), this);
        portInput1->setPlaceholderText("Enter port 1");
        portInput1->setStyleSheet(
            "QLineEdit {"
            "    font-size: 32px;"
            "    padding: 5px;"
            "    border: 1px solid #999;"
            "    border-radius: 3px;"
            "}"
            "QLineEdit:focus {"
            "    border: 2px solid #008CBA;"
            "}"
        );
        
        QLabel* portLabel2 = new QLabel("Port 2:", this);
        portLabel2->setStyleSheet("font-size: 32px;");
        
        QLineEdit* portInput2 = new QLineEdit(QString::number(backends[i].ports[1]), this);
        portInput2->setPlaceholderText("Enter port 2");
        portInput2->setStyleSheet(
            "QLineEdit {"
            "    font-size: 32px;"
            "    padding: 5px;"
            "    border: 1px solid #999;"
            "    border-radius: 3px;"
            "}"
            "QLineEdit:focus {"
            "    border: 2px solid #008CBA;"
            "}"
        );
        
        configLayout->addWidget(ipLabel, i, 0);
        configLayout->addWidget(ipInput, i, 1);
        configLayout->addWidget(portLabel1, i, 2);
        configLayout->addWidget(portInput1, i, 3);
        configLayout->addWidget(portLabel2, i, 4);
        configLayout->addWidget(portInput2, i, 5);
        
        ipInputs.push_back(ipInput);
        portInputs1.push_back(portInput1);
        portInputs2.push_back(portInput2);
    }

    // Add apply button
    applyBtn = new QPushButton("Apply Configuration", this);
    applyBtn->setMinimumSize(200, 50);
    applyBtn->setStyleSheet(
        "QPushButton {"
        "    font-size: 32px;"
        "    font-weight: bold;"
        "    padding: 5px;"
        "    background-color: #008CBA;"
        "    color: white;"
        "    border-radius: 5px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #007399;"
        "}"
    );
    connect(applyBtn, &QPushButton::clicked, this, &ControlApp::applyConfiguration);
    configLayout->addWidget(applyBtn, backends.size(), 0, 1, 6);

    configGroup->setLayout(configLayout);
    mainLayout->addWidget(configGroup);

    // Create control group
    QGroupBox* controlGroup = new QGroupBox("Control Panel", this);
    QGridLayout* controlLayout = new QGridLayout;

    // Create status labels
    for (size_t i = 0; i < backends.size(); ++i) {
        QLabel* label = new QLabel(QString::fromStdString(backends[i].name + ": Not Connected"), this);
        label->setStyleSheet("color: red; font-size: 32px;");
        controlLayout->addWidget(label, 0, i, Qt::AlignCenter);
        statusLabels.push_back(label);
    }

    // Create buttons
    toggleBtn = new QPushButton("Start", this);
    toggleBtn->setMinimumSize(200, 50);
    toggleBtn->setStyleSheet(
        "QPushButton {"
        "    font-size: 32px;"
        "    font-weight: bold;"
        "    padding: 5px;"
        "    background-color: #4CAF50;"
        "    color: white;"
        "    border-radius: 5px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #45a049;"
        "}"
    );
    connect(toggleBtn, &QPushButton::clicked, this, &ControlApp::toggleAction);

    eventBtn = new QPushButton("Send Event", this);
    eventBtn->setMinimumSize(200, 50);
    eventBtn->setStyleSheet(
        "QPushButton {"
        "    font-size: 32px;"
        "    font-weight: bold;"
        "    padding: 5px;"
        "    background-color: #008CBA;"
        "    color: white;"
        "    border-radius: 5px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #007399;"
        "}"
        "QPushButton:disabled {"
        "    background-color: #cccccc;"
        "    color: #666666;"
        "}"
    );
    connect(eventBtn, &QPushButton::clicked, this, &ControlApp::sendEvent);

    controlLayout->addWidget(toggleBtn, 1, 0, 1, 2, Qt::AlignCenter);
    controlLayout->addWidget(eventBtn, 2, 0, 1, 2, Qt::AlignCenter);

    controlGroup->setLayout(controlLayout);
    mainLayout->addWidget(controlGroup);

    // Set window properties
    setMinimumSize(1200, 800);
    resize(1200, 800);
    centerWindow();
    eventBtn->setEnabled(true);

    // Set window style
    setStyleSheet(
        "QGroupBox {"
        "    font-size: 32px;"
        "    font-weight: bold;"
        "    margin-top: 1ex;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    subcontrol-position: top center;"
        "    padding: 0 3px;"
        "}"
        "QLineEdit {"
        "    padding: 5px;"
        "    border: 1px solid #999;"
        "    border-radius: 3px;"
        "}"
    );
}

void ControlApp::toggleAction() {
    if (!isToggleOn) {  // Sending START
        std::vector<bool> results;
        int idx = 0;
        for (auto& backend : tcpClient->getBackends()) {
            results.push_back(tcpClient->sendLoggingMessage(MessageType::START, backend, idx++));
        }
        if (std::all_of(results.begin(), results.end(), [](bool result) { return result; })) {
            toggleBtn->setText("End");
            toggleBtn->setStyleSheet(
                "QPushButton {"
                "    font-size: 32px;"
                "    font-weight: bold;"
                "    padding: 5px;"
                "    background-color: #ff9999;"
                "    color: white;"
                "    border-radius: 5px;"
                "}"
                "QPushButton:hover {"
                "    background-color: #ff8080;"
                "}"
            );
            isToggleOn = true;
            eventBtn->setEnabled(false);
        }
    } else {  // Sending END
        std::vector<bool> results;
        int idx = 0;
        for (auto& backend : tcpClient->getBackends()) {
            results.push_back(tcpClient->sendLoggingMessage(MessageType::STOP, backend, idx++));
        }
        if (std::all_of(results.begin(), results.end(), [](bool result) { return result; })) {
            toggleBtn->setText("Start");
            toggleBtn->setStyleSheet(
                "QPushButton {"
                "    font-size: 32px;"
                "    font-weight: bold;"
                "    padding: 5px;"
                "    background-color: #4CAF50;"
                "    color: white;"
                "    border-radius: 5px;"
                "}"
                "QPushButton:hover {"
                "    background-color: #45a049;"
                "}"
            );
            isToggleOn = false;
            eventBtn->setEnabled(true);
        }
    }
}

void ControlApp::sendEvent() {
    std::vector<bool> results;
    int idx = 0;
    for (auto& backend : tcpClient->getBackends()) {
        results.push_back(tcpClient->sendLoggingMessage(MessageType::EVENT, backend, idx++));
    }
    if (std::all_of(results.begin(), results.end(), [](bool result) { return result; })) {
        eventBtn->setEnabled(false);
        timer->start(30000);  // 30 seconds
    }
}

void ControlApp::enableEventButton() {
    eventBtn->setEnabled(true);
    timer->stop();
}

void ControlApp::centerWindow() {
    QRect screenGeometry = QApplication::desktop()->screenGeometry();
    int x = (screenGeometry.width() - width()) / 2;
    int y = (screenGeometry.height() - height()) / 2;
    move(x, y);
}

void ControlApp::applyConfiguration() {
    bool allValid = true;
    std::string errorMessage;

    auto& backends = tcpClient->getBackends();
    for (size_t i = 0; i < backends.size(); ++i) {
        QString ip = ipInputs[i]->text().trimmed();
        QString port1 = portInputs1[i]->text().trimmed();
        QString port2 = portInputs2[i]->text().trimmed();
        
        // Check if IP is empty
        if (ip.isEmpty()) {
            errorMessage = backends[i].name + ": IP address cannot be empty";
            allValid = false;
            break;
        }

        // Validate IP format
        QRegExp ipRegex("^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
        if (!ipRegex.exactMatch(ip)) {
            errorMessage = backends[i].name + ": Invalid IP address format";
            allValid = false;
            break;
        }

        // Validate ports
        bool ok1, ok2;
        int portNum1 = port1.toInt(&ok1);
        int portNum2 = port2.toInt(&ok2);
        
        if (!ok1 || !ok2 || portNum1 < 1 || portNum1 > 65535 || portNum2 < 1 || portNum2 > 65535) {
            errorMessage = backends[i].name + ": Invalid port number (must be between 1 and 65535)";
            allValid = false;
            break;
        }

        // Check if configuration has changed
        if (ip.toStdString() == backends[i].host && 
            portNum1 == backends[i].ports[0] && 
            portNum2 == backends[i].ports[1]) {
            continue;  // Skip if configuration hasn't changed
        }

        // Update configuration
        backends[i].host = ip.toStdString();
        backends[i].ports[0] = portNum1;
        backends[i].ports[1] = portNum2;
        tcpClient->cleanupSockets();
    }

    if (!allValid) {
        QMessageBox::warning(this, "Configuration Error", QString::fromStdString(errorMessage));
        return;
    }

    // Reconnect to servers with new configuration
    connectToServer();
    QMessageBox::information(this, "Success", "Configuration applied successfully");
}

void ControlApp::connectToServer() {
    if (tcpClient->connectToServer()) {
        statusTimer->stop();
        eventSent = false;
        eventBtn->setEnabled(true);
        toggleBtn->setEnabled(true);

        auto& backends = tcpClient->getBackends();
        for (size_t i = 0; i < backends.size(); ++i) {
            statusLabels[i]->setText(QString::fromStdString(backends[i].name + ": Connected"));
            statusLabels[i]->setStyleSheet("color: green; font-size: 32px;");
        }

        // Initialize data threads
        dataThread = std::thread([this]() {
            while (!stopDataThread || !serverConnected) {
                if (serverConnected) {
                    tcpClient->sendData();
                    // std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    break;
                }
            }
        });

        receiveThread = std::thread([this]() {
            while (!stopReceiveThread || !serverConnected) {
                if (serverConnected) {
                    tcpClient->receiveData();
                    break;
                }
            }
        });

    } else {
        auto& backends = tcpClient->getBackends();
        for (size_t i = 0; i < backends.size(); ++i) {
            statusLabels[i]->setText(QString::fromStdString(backends[i].name + ": Not Connected"));
            statusLabels[i]->setStyleSheet("color: red; font-size: 32px;");
        }
    }
}

void ControlApp::closeEvent(QCloseEvent* event) {
    tcpClient->cleanupSockets();
    event->accept();
}

void ControlApp::processData(const char* imageData, int sensorType, int width, int height) {
    
    if (sensorType == 1) {
        // YUV422UYVY를 RGB로 변환
        cv::Mat yuv(height, width, CV_8UC2, const_cast<void*>(static_cast<const void*>(imageData)));
        cv::Mat rgb;
        cv::cvtColor(yuv, rgb, cv::COLOR_YUV2BGR_UYVY);
        
        // QImage로 변환
        QImage img(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
        imageViewer->updateImage(0, rgb);
    }
}
