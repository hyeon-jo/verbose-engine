#include "control_app.hpp"
#include <QApplication>
#include <QDesktopWidget>
#include <QMessageBox>
#include <QRegExp>
#include <thread>
#include <mutex>

ConnectionThread::ConnectionThread(TcpClient* client, std::vector<Backend>& backends, QObject* parent)
    : QThread(parent)
    , tcpClient(client)
    , backends(backends)
    , stopRequested(false)
    , shouldConnect(false)
{
}

void ConnectionThread::stop()
{
    stopRequested = true;
    wait();
}

void ConnectionThread::startConnection()
{
    shouldConnect = true;
}

void ConnectionThread::run()
{
    while (!stopRequested) {
        if (shouldConnect) {
            QMutexLocker locker(&mutex);
            tcpClient->checkConnections(backends);
            shouldConnect = false;
        }
        msleep(100);  // 100ms 대기
    }
}

ControlApp::ControlApp(QWidget* parent)
    : QMainWindow(parent)
    , isToggleOn(false)
    , eventSent(false)
    , tcpClient(std::make_unique<TcpClient>())
{
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

    // Set up TCP client callbacks
    tcpClient->setCallbacks(
        [this](const Backend& backend, bool connected) { onConnectionStatusChanged(backend, connected); },
        [this](const Backend& backend, bool success) { onMessageStatusChanged(backend, success); }
    );

    setupUI();

    // Setup timers
    timer = new QTimer(this);
    QObject::connect(timer, &QTimer::timeout, this, &ControlApp::enableEventButton);

    statusTimer = new QTimer(this);
    QObject::connect(statusTimer, &QTimer::timeout, this, &ControlApp::connectToServer);
    statusTimer->start(5000);  // Check every 5 seconds

    // Create and start connection thread
    connectionThread = new ConnectionThread(tcpClient.get(), backends, this);
    connectionThread->start();
}

ControlApp::~ControlApp() {
    if (connectionThread) {
        connectionThread->stop();
        delete connectionThread;
    }
    cleanupConnections();
}

void ControlApp::cleanupConnections() {
    for (auto& backend : backends) {
        tcpClient->disconnect(backend);
    }
}

void ControlApp::closeEvent(QCloseEvent* event) {
    cleanupConnections();
    event->accept();
}

void ControlApp::connectToServer() {
    if (connectionThread) {
        connectionThread->startConnection();
    }
}

void ControlApp::onConnectionStatusChanged(const Backend& backend, bool connected) {
    updateStatusLabel(backend, connected);
    checkAllConnected();
}

void ControlApp::onMessageStatusChanged(const Backend& backend, bool success) {
    if (!success) {
        QMessageBox::warning(this, "Message Error",
            QString::fromStdString(backend.name + ": Failed to send message"));
    }
}

void ControlApp::updateStatusLabel(const Backend& backend, bool connected) {
    for (size_t i = 0; i < backends.size(); ++i) {
        if (backends[i].name == backend.name) {
            QString status = QString::fromStdString(backend.name + ": ") +
                           (connected ? "Connected" : "Not Connected");
            statusLabels[i]->setText(status);
            statusLabels[i]->setStyleSheet(
                connected ? "color: green; font-size: 32px;" : "color: red; font-size: 32px;"
            );
            break;
        }
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
        tcpClient->disconnect(backends[i]);
        backends[i].host = ip.toStdString();
        backends[i].ports[0] = portNum1;
        backends[i].ports[1] = portNum2;
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
    bool allSuccess = true;
    isToggleOn = !isToggleOn;

    for (auto& backend : backends) {
        for (int i = 0; i < 2; ++i) {
            if (!tcpClient->sendMessage(isToggleOn ? MessageType::start : MessageType::stop, backend, i)) {
                allSuccess = false;
            }
        }
    }

    if (allSuccess) {
        toggleBtn->setText(isToggleOn ? "Stop" : "Start");
        toggleBtn->setStyleSheet(
            isToggleOn ?
            "QPushButton {"
            "    font-size: 32px;"
            "    font-weight: bold;"
            "    padding: 5px;"
            "    background-color: #f44336;"
            "    color: white;"
            "    border-radius: 5px;"
            "}"
            "QPushButton:hover {"
            "    background-color: #da190b;"
            "}" :
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

        if (isToggleOn) {
            eventBtn->setEnabled(true);
            eventSent = false;
        } else {
            eventBtn->setEnabled(false);
            timer->stop();
        }
    } else {
        isToggleOn = !isToggleOn;  // Revert the toggle state
        QMessageBox::warning(this, "Error", "Failed to send command to all backends");
    }
}

void ControlApp::sendEvent() {
    bool allSuccess = true;

    for (auto& backend : backends) {
        for (int i = 0; i < 2; ++i) {
            if (!tcpClient->sendMessage(MessageType::event, backend, i)) {
                allSuccess = false;
            }
        }
    }

    if (allSuccess) {
        eventBtn->setEnabled(false);
        eventSent = true;
        timer->start(30000);  // 30 seconds timeout
    } else {
        QMessageBox::warning(this, "Error", "Failed to send event to all backends");
    }
}

void ControlApp::enableEventButton() {
    if (eventSent) {
        eventBtn->setEnabled(true);
        eventSent = false;
        timer->stop();
    }
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

void ControlApp::centerWindow() {
    QRect screenGeometry = QApplication::desktop()->screenGeometry();
    int x = (screenGeometry.width() - width()) / 2;
    int y = (screenGeometry.height() - height()) / 2;
    move(x, y);
}

void ControlApp::checkAllConnected() {
    bool allConnected = true;
    for (const auto& backend : backends) {
        if (!backend.ready) {
            allConnected = false;
            break;
        }
    }

    if (allConnected && !sensorWindow) {
        sensorWindow = std::make_unique<SensorWindow>();
        sensorWindow->show();
        this->hide();  // 기존 설정 창 숨기기
    }
} 