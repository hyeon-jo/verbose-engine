#include "control_app.hpp"
#include <QApplication>
#include <QDesktopWidget>
#include <QMessageBox>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <iostream>

#include "messages.hpp"

using boost::asio::ip::tcp;
using boost::system::error_code;

namespace {
    // Helper function to convert string to boost::asio::ip::address
    boost::asio::ip::address to_address(const std::string& host) {
        return boost::asio::ip::make_address(host);
    }
}

ControlApp::ControlApp(QWidget* parent) : QMainWindow(parent), 
    isToggleOn(false), eventSent(false), messageCounter(0),
    io_context(std::make_shared<boost::asio::io_context>()) {
    
    // Initialize backends
    backends = {
        {
            "127.0.0.1",
            {9090, 9091},
            "Backend 1",
            false,
            {nullptr, nullptr}
        },
        {
            "127.0.0.1",
            {9092, 9093},
            "Backend 2",
            false,
            {nullptr, nullptr}
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
            // Create socket
            backends[i].sockets[0] = std::make_shared<tcp::socket>(*io_context);
            
            // Resolve endpoint
            tcp::endpoint endpoint(to_address(backends[i].host), backends[i].ports[0]);
            
            // Connect synchronously
            backends[i].sockets[0]->connect(endpoint);

            // Store socket
            backends[i].ready = true;
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
            try {
                backend.sockets[1] = std::make_shared<tcp::socket>(*io_context);
                tcp::endpoint endpoint(to_address(backend.host), backend.ports[1]);
                
                // Connect synchronously
                backend.sockets[1]->connect(endpoint);
                sendTcpMessage(MessageType::configInfo);
            } catch (const std::exception& e) {
                std::cerr << "Error connecting to second port: " << e.what() << std::endl;
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
        if (sendTcpMessage(MessageType::start)) {
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
        if (sendTcpMessage(MessageType::stop)) {
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

bool ControlApp::setMessage(stDataRecordConfigMsg& msg, uint8_t messageType) {
    msg.header.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    msg.header.messageType = messageType;
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

bool ControlApp::sendTcpMessage(uint8_t messageType) {
    stDataRecordConfigMsg msg;
    if (!setMessage(msg, messageType)) {
        return false;
    }

    std::ostringstream archive_stream;
    boost::archive::text_oarchive archive(archive_stream);
    archive << msg;

    std::string outbound_data_ = archive_stream.str();
    std::ostringstream header_stream;
    header_stream << std::setw(header_length) << std::hex << outbound_data_.size();

    if (!header_stream || header_stream.str().size() != header_length) {
        std::cerr << "Incorrect header length" << std::endl;
        return false;
    }
    std::string outbound_header_ = header_stream.str();

    std::cout << "Outbound header: " << outbound_header_ << std::endl;
    std::cout << "Outbound data: " << outbound_data_ << std::endl;

    std::vector<boost::asio::const_buffer> buffers;
    buffers.push_back(boost::asio::buffer(outbound_header_));
    buffers.push_back(boost::asio::buffer(outbound_data_));

    for (auto& backend : backends) {
        if (backend.ready && backend.sockets[1]->is_open()) {
            auto& socket = backend.sockets[1];
            try {
                // Get executor from socket
                auto executor = socket->get_executor();
                        
                    // Create completion handler
                    auto handler = [](const boost::system::error_code& error, std::size_t bytes_transferred) {
                        if (error) {
                            std::cerr << "Async write error: " << error.message() << std::endl;
                        } else {
                            std::cout << "Async write completed: " << bytes_transferred << " bytes" << std::endl;
                        }
                    };

                    // Start async write
                    boost::asio::async_write(*socket, 
                        boost::asio::buffer(buffers),
                        boost::asio::bind_executor(executor, handler));

            } catch (const std::exception& e) {
                std::cerr << "Error sending message: " << e.what() << std::endl;
                socket = nullptr;
                return false;
            }
        }
        else if (backend.ready) {
            std::cerr << "Socket is closed" << std::endl;
            return false;
        }
        else {
            std::cerr << "Backend is not ready" << std::endl;
            return false;
        }
    }
    return true;
}

void ControlApp::cleanupSockets() {
    for (auto& backend : backends) {
        for (auto& socket : backend.sockets) {
            if (socket && socket->is_open()) {
                try {
                    socket->close();
                } catch (const std::exception& e) {
                    std::cerr << "Error closing socket: " << e.what() << std::endl;
                }
                socket = nullptr;
            }
        }
        backend.ready = false;
    }
}

void ControlApp::closeEvent(QCloseEvent* event) {
    cleanupSockets();
    event->accept();
} 