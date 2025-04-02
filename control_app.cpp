#include "control_app.hpp"
#include <QApplication>
#include <QDesktopWidget>
#include <QMessageBox>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <ctime>
#include <iostream>

#include "messages.hpp"

namespace {
    // Helper functions to avoid namespace conflicts
    int sys_connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
        return ::connect(sockfd, addr, addrlen);
    }

    int sys_close(int fd) {
        return ::close(fd);
    }
}

ControlApp::ControlApp(QWidget* parent) : QMainWindow(parent), 
    isToggleOn(false), eventSent(false), messageCounter(0) {
    
    // Initialize backends
    backends = {
        {
            "127.0.0.1",
            {9090, 9091},
            "Backend 1",
            false,
            {-1, -1}
        },
        {
            "127.0.0.1",
            {9092, 9093},
            "Backend 2",
            false,
            {-1, -1}
        }
    };

    setupUI();

    // Setup timers
    timer = new QTimer(this);
    QObject::connect(timer, &QTimer::timeout, this, &ControlApp::enableEventButton);

    statusTimer = new QTimer(this);
    QObject::connect(statusTimer, &QTimer::timeout, this, &ControlApp::connectToServer);
    statusTimer->start(1000);  // Check every 1 second
}

ControlApp::~ControlApp() {
    cleanupSockets();
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
    for (size_t i = 0; i < backends.size(); ++i) {
        QLabel* ipLabel = new QLabel(QString::fromStdString(backends[i].name + " IP:"), this);
        ipLabel->setStyleSheet("font-size: 32px;");
        
        QLineEdit* ipInput = new QLineEdit(QString::fromStdString(backends[i].host), this);
        ipInput->setPlaceholderText("Enter IP address");
        ipInput->setStyleSheet("font-size: 32px; padding: 5px;");
        
        configLayout->addWidget(ipLabel, i, 0);
        configLayout->addWidget(ipInput, i, 1);
        ipInputs.push_back(ipInput);

        QLabel* portLabel = new QLabel(QString("Ports: %1, %2")
            .arg(backends[i].ports[0])
            .arg(backends[i].ports[1]), this);
        portLabel->setStyleSheet("font-size: 32px;");
        configLayout->addWidget(portLabel, i, 2);
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
    configLayout->addWidget(applyBtn, backends.size(), 0, 1, 4);

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

void ControlApp::connectToServer() {
    bool allConnected = true;

    for (size_t i = 0; i < backends.size(); ++i) {
        uint32_t prevCounter = messageCounter;
        try {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                throw std::runtime_error("Socket creation failed");
            }

            struct sockaddr_in serverAddr;
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(backends[i].ports[0]);
            if (inet_pton(AF_INET, backends[i].host.c_str(), &serverAddr.sin_addr) <= 0) {
                sys_close(sock);
                throw std::runtime_error("Invalid address");
            }

            if (sys_connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
                sys_close(sock);
                throw std::runtime_error("Connection failed");
            }

            // Handle connection protocol here...
            backends[i].ready = true;
            backends[i].sockets[0] = sock;
            statusLabels[i]->setText(QString::fromStdString(backends[i].name + ": Connected"));
            statusLabels[i]->setStyleSheet("color: green; font-size: 32px;");

        } catch (const std::exception& e) {
            std::cerr << "Error with " << backends[i].name << ":" 
                      << backends[i].ports[0] << ": " << e.what() << std::endl;
            allConnected = false;
            statusLabels[i]->setText(QString::fromStdString(backends[i].name + ": Not Connected"));
            statusLabels[i]->setStyleSheet("color: red; font-size: 32px;");
            messageCounter = prevCounter;
            continue;
        }
    }

    if (allConnected) {
        statusTimer->stop();
        eventSent = false;
        eventBtn->setEnabled(true);
        toggleBtn->setEnabled(true);

        // Connect second sockets
        for (auto& backend : backends) {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock >= 0) {
                struct sockaddr_in serverAddr;
                serverAddr.sin_family = AF_INET;
                serverAddr.sin_port = htons(backend.ports[1]);
                if (inet_pton(AF_INET, backend.host.c_str(), &serverAddr.sin_addr) > 0) {
                    if (sys_connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) >= 0) {
                        backend.sockets[1] = sock;
                        continue;
                    }
                }
                sys_close(sock);
            }
        }
        std::cout << "All backends connected successfully" << std::endl;
    }
}

void ControlApp::applyConfiguration() {
    for (size_t i = 0; i < backends.size(); ++i) {
        QString ip = ipInputs[i]->text().trimmed();
        if (ip.isEmpty()) {
            QMessageBox::warning(this, "Configuration Error", 
                QString::fromStdString(backends[i].name + ": IP address cannot be empty"));
            return;
        }
        
        backends[i].host = ip.toStdString();
        cleanupSockets();
    }
    
    connectToServer();
    QMessageBox::information(this, "Success", "Configuration applied successfully");
}

void ControlApp::toggleAction() {
    if (!isToggleOn) {  // Sending START
        if (sendTcpMessage(true)) {
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
        if (sendTcpMessage(false)) {
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
    // TODO: Implement event sending logic
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

bool ControlApp::setMessage(stDataRecordConfigMsg& msg, bool start) {
    msg.header.messageType = start ? 19 : 21;
    msg.header.sequenceNumber = messageCounter++;
    msg.header.bodyLength = sizeof(msg);

    std::vector<stLoggingFile> loggingFileList;
    stLoggingFile loggingFile;
    loggingFile.id = 0;
    loggingFile.enable = "true";
    loggingFile.namePrefix = "logging";
    loggingFile.nameSubfix = "";
    loggingFile.extension = ".log";
    loggingFileList.push_back(loggingFile);

    stMetaData metaData;
    metaData.data["control_app"] = "control_app";
    metaData.issue = "control_app";

    msg.loggingDirectoryPath = "/work/data/logging";
    msg.loggingMode = 0;
    msg.historyTime = 1;
    msg.followTime = 1;
    msg.splitTime = 1;
    msg.dataLength = 1;
    msg.loggingFileList = loggingFileList;
    msg.metaData = metaData;
    
    return true;
}

bool ControlApp::sendTcpMessage(bool start) {
    stDataRecordConfigMsg msg;
    if (!setMessage(msg, start)) {
        return false;
    }

    std::vector<uint8_t> buffer;
    buffer.resize(sizeof(msg));
    std::memcpy(buffer.data(), &msg, sizeof(msg));

    for (auto& backend : backends) {
        if (backend.ready) {
            for (int& sock : backend.sockets) {
                if (sock >= 0) {
                    send(sock, buffer.data(), buffer.size(), 0);
                }
            }
        }
    }
    return true;
}

void ControlApp::cleanupSockets() {
    for (auto& backend : backends) {
        for (int& sock : backend.sockets) {
            if (sock >= 0) {
                sys_close(sock);
                sock = -1;
            }
        }
        backend.ready = false;
    }
}

void ControlApp::closeEvent(QCloseEvent* event) {
    cleanupSockets();
    event->accept();
} 