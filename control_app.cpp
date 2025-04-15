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
            "192.168.10.10",
            {9090, 9091},
            "Backend 1",
            false,
            {nullptr, nullptr}
        },
        {
            "192.168.10.20",
            {9090, 9091},
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
    statusTimer->start(1000);  // Check every 5 seconds
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
        int idx = 0;
        for (auto& backend : backends) {
            try {
                backend.sockets[1] = std::make_shared<tcp::socket>(*io_context);
                tcp::endpoint endpoint(to_address(backend.host), backend.ports[1]);
                
                // Connect synchronously
                backend.sockets[1]->connect(endpoint);
                sendMessage(MessageType::CONFIG_INFO, backend, idx++);
            } catch (const std::exception& e) {
                std::cerr << "Error connecting to second port: " << e.what() << std::endl;
            }
        }
        std::cout << "All backends connected successfully" << std::endl;
        sendMessage(MessageType::DATA_SEND_REQUEST, backends[0], 0);
    }
}

void ControlApp::applyConfiguration() {
    bool allValid = true;
    std::string errorMessage;

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
        cleanupSockets();
    }

    if (!allValid) {
        QMessageBox::warning(this, "Configuration Error", QString::fromStdString(errorMessage));
        return;
    }

    // Reconnect to servers with new configuration
    connectToServer();
    QMessageBox::information(this, "Success", "Configuration applied successfully");
}

void ControlApp::toggleAction() {
    if (!isToggleOn) {  // Sending START
        std::vector<bool> results;
        int idx = 0;
        for (auto& backend : backends) {
            results.push_back(sendMessage(MessageType::START, backend, idx++));
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
        else {
            for (int i = 0; i < results.size(); i++) {
                if (results[i]) {
                    sendMessage(MessageType::STOP, backends[i], i);
                }
            }
        }
    } else {  // Sending END
        std::vector<bool> results;
        int idx = 0;
        for (auto& backend : backends) {
            results.push_back(sendMessage(MessageType::STOP, backend, idx++));
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
        else {
            for (int i = 0; i < results.size(); i++) {
                if (!results[i]) {
                    sendMessage(MessageType::STOP, backends[i], i);
                }
            }
        }
    }
}

void ControlApp::sendEvent() {
    std::vector<bool> results;
    int idx = 0;
    QTimer::singleShot(30000, [this, &results, &idx]() {
        for (auto& backend : backends) {
            results.push_back(sendMessage(MessageType::EVENT, backend, idx++));
        }
        if (std::all_of(results.begin(), results.end(), [](bool result) { return result; })) {
            eventBtn->setEnabled(false);
        }
        else {
            // pass
        }
    });
    eventBtn->setEnabled(true);
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

Header ControlApp::setHeader(uint8_t messageType) {
    Header header;
    header.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    header.messageType = messageType;
    if (messageType == MessageType::DATA_SEND_REQUEST) {
        messageCounter++;
    }
    header.sequenceNumber = messageCounter;
    header.bodyLength = 0;

    return header;
}

bool ControlApp::setRecordConfigMessage(stDataRecordConfigMsg& msg, uint8_t messageType) {
    msg.header = setHeader(messageType);

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

    msg.loggingDirectoryPath = "/home/nvidia/Work/data/logging";
    msg.loggingMode = 0;
    msg.historyTime = 1;
    msg.followTime = 1;
    msg.splitTime = 1;
    msg.dataLength = 1;
    msg.loggingFileList = loggingFileList;
    msg.metaData = metaData;
    
    return true;
}

bool ControlApp::sendMessage(uint8_t messageType, Backend& backend, int idx) {

    std::ostringstream archive_stream;
    boost::archive::text_oarchive archive(archive_stream);
    int socketIdx = 0;

    if (messageType == MessageType::CONFIG_INFO
        || messageType == MessageType::START
        || messageType == MessageType::EVENT
        || messageType == MessageType::STOP)
    {
        stDataRecordConfigMsg msg;
        if (!setRecordConfigMessage(msg, messageType)) {
            return false;
        }
        archive << msg;
        socketIdx = 1;
    }
    else
    {
        stDataRequestMsg msg;
        if (!setTCPMessage(msg, messageType)) {
            return false;
        }
        msg.mDataType = eDataType::SENSOR;
        msg.mSensorChannel = getSensorChannelBitmask(eSensorChannel::CAMERA_FRONT);
        archive << msg;
        socketIdx = 0;
    }

    std::cout << "Socket index: " << socketIdx << std::endl;

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

    if (backend.ready && backend.sockets[socketIdx]->is_open()) {
        auto& socket = backend.sockets[socketIdx];
        try {
            // Get executor from socket
            auto executor = socket->get_executor();
                    
                // Create completion handler
                auto handler = [this, &backend, idx](const boost::system::error_code& error, std::size_t bytes_transferred) {
                    if (error) {
                        std::cerr << "Async write error: " << error.message() << std::endl;
                        statusLabels[idx]->setText(QString::fromStdString(backend.name + ": Not Connected"));
                        statusLabels[idx]->setStyleSheet("color: red; font-size: 32px;");
                    } else {
                        std::cout << "Async write completed: " << bytes_transferred << " bytes" << std::endl;
                    }
                };

                // Start async write
                boost::asio::async_write(*socket, 
                    boost::asio::buffer(outbound_header_),
                    boost::asio::bind_executor(executor, handler));

                boost::asio::async_write(*socket, 
                    boost::asio::buffer(outbound_data_),
                    boost::asio::bind_executor(executor, handler));

        } catch (const std::exception& e) {
            std::cerr << "Error sending message: " << e.what() << std::endl;
            socket = nullptr;
            return false;
        }
    }
    else if (backend.ready) {
        std::cerr << "Socket is closed" << std::endl;
        backend.ready = false;
        statusLabels[idx]->setText(QString::fromStdString(backend.name + ": Not Connected"));
        statusLabels[idx]->setStyleSheet("color: red; font-size: 32px;");
        
        return false;
    }
    else {
        std::cerr << "Backend is not ready" << std::endl;
        return false;
    }
    return true;
}

bool ControlApp::setTCPMessage(stDataRequestMsg& msg, uint8_t messageType) {
    msg.header = setHeader(messageType);
    msg.mRequestStatus = 0;
    // msg.mDataType = getSensorChannelBitmask(eSensorChannel::CAMERA_FRONT);
    msg.mSensorChannel = 0;
    msg.mServiceID = 0;
    msg.mNetworkID = 0;

    return true;
}

bool ControlApp::getSensorDataFromServer(Backend& backend, int idx) {
    auto executor = backend.sockets[0]->get_executor();
    char* headerBuffer = new char[21];
    char* dataBuffer = nullptr;
    if(!sendMessage(MessageType::LINK, backend, idx))
    {
        return false;
    }
    boost::asio::async_read(*backend.sockets[0], boost::asio::buffer(headerBuffer, 21),
        boost::asio::bind_executor(executor, [this, &backend, idx](const boost::system::error_code& error, std::size_t bytes_transferred) {
            if (error) {
                std::cerr << "Async read error: " << error.message() << std::endl;
            }
        }));
    Header linkAckHeader = *reinterpret_cast<Header*>(headerBuffer);
    if (linkAckHeader.messageType == MessageType::LINK_ACK) {
        while (true) {
            if(!sendMessage(MessageType::DATA_SEND_REQUEST, backend, idx)) {
                return false;
            }
            boost::asio::async_read(*backend.sockets[0], boost::asio::buffer(headerBuffer, 21),
                boost::asio::bind_executor(executor, [this, &backend, idx](const boost::system::error_code& error, std::size_t bytes_transferred) {
                    if (error) {
                        std::cerr << "Async read error: " << error.message() << std::endl;
                    }
                }));
            Header retDataHeader = *reinterpret_cast<Header*>(headerBuffer);
            dataBuffer = new char[retDataHeader.bodyLength];
            boost::asio::async_read(*backend.sockets[0], boost::asio::buffer(dataBuffer, retDataHeader.bodyLength),
                boost::asio::bind_executor(executor, [this, &backend, idx](const boost::system::error_code& error, std::size_t bytes_transferred) {
                    if (error) {
                        std::cerr << "Async read error: " << error.message() << std::endl;
                    }
                }));
        }
    }
    else {
        return false;
    }
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