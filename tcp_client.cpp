#include "tcp_client.hpp"
#include <boost/asio.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <iostream>

using boost::asio::ip::tcp;
using boost::system::error_code;

namespace {
    boost::asio::ip::address to_address(const std::string& host) {
        return boost::asio::ip::make_address(host);
    }
}

TcpClient::TcpClient() : messageCounter(0),
    io_context(std::make_shared<boost::asio::io_context>()) {
    initializeBackends();
}

TcpClient::~TcpClient() {
    cleanupSockets();
}

void TcpClient::initializeBackends() {
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
}

void TcpClient::cleanupSockets() {
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

bool TcpClient::connectToServer() {
    bool allConnected = true;

    for (size_t i = 0; i < backends.size(); ++i) {
        uint32_t prevCounter = messageCounter;
        try {
            backends[i].sockets[0] = std::make_shared<tcp::socket>(*io_context);
            tcp::endpoint endpoint(to_address(backends[i].host), backends[i].ports[0]);
            backends[i].sockets[0]->connect(endpoint);
            backends[i].ready = true;
        } catch (const std::exception& e) {
            std::cerr << "Error with " << backends[i].name << ":" 
                      << backends[i].ports[0] << ": " << e.what() << std::endl;
            allConnected = false;
            messageCounter = prevCounter;
            continue;
        }
    }

    if (allConnected) {
        int idx = 0;
        for (auto& backend : backends) {
            try {
                backend.sockets[1] = std::make_shared<tcp::socket>(*io_context);
                tcp::endpoint endpoint(to_address(backend.host), backend.ports[1]);
                backend.sockets[1]->connect(endpoint);
                sendLoggingMessage(MessageType::CONFIG_INFO, backend, idx++);
            } catch (const std::exception& e) {
                std::cerr << "Error connecting to second port: " << e.what() << std::endl;
            }
        }
        std::cout << "All backends connected successfully" << std::endl;
    }

    return allConnected;
}

Header TcpClient::setHeader(uint8_t messageType) {
    Header header;
    header.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    header.messageType = messageType;
    if (messageType == MessageType::DATA_SEND_REQUEST) {
        messageCounter++;
    }
    header.sequenceNumber = messageCounter;
    header.bodyLength = 1;

    return header;
}

void TcpClient::writeHeader(Backend& backend, MessageType msgType) {
    char* headerBuffer = new char[22];
    Header sendHeader = setHeader(msgType);
    int offset = 0;

    memcpy(headerBuffer + offset, &sendHeader.timestamp, sizeof(sendHeader.timestamp));
    offset += sizeof(sendHeader.timestamp);
    memcpy(headerBuffer + offset, &sendHeader.messageType, sizeof(sendHeader.messageType));
    offset += sizeof(sendHeader.messageType);
    memcpy(headerBuffer + offset, &sendHeader.sequenceNumber, sizeof(sendHeader.sequenceNumber));
    offset += sizeof(sendHeader.sequenceNumber);
    memcpy(headerBuffer + offset, &sendHeader.bodyLength, sizeof(sendHeader.bodyLength));
    offset += sizeof(sendHeader.bodyLength);

    boost::asio::write(*backend.sockets[0], boost::asio::buffer(headerBuffer, 22));
}

void TcpClient::parseHeader(char* headerBuffer, Header& header) {
    int offset = 0;
    memcpy(&header.timestamp, headerBuffer + offset, sizeof(header.timestamp));
    offset += sizeof(header.timestamp);
    memcpy(&header.messageType, headerBuffer + offset, sizeof(header.messageType));
    offset += sizeof(header.messageType);
    memcpy(&header.sequenceNumber, headerBuffer + offset, sizeof(header.sequenceNumber));
    offset += sizeof(header.sequenceNumber);
    memcpy(&header.bodyLength, headerBuffer + offset, sizeof(header.bodyLength));
    offset += sizeof(header.bodyLength);
}

Protocol_Header TcpClient::getReceivedHeader(Backend& backend, int idx) {
    usleep(300000);
    char* headerBuffer = new char[sizeof(Protocol_Header)];
    int offset = 0;
    auto executor = backend.sockets[0]->get_executor();
    auto handler = [this, idx](const boost::system::error_code& error, std::size_t bytes_transferred) {
        if (error) {
            std::cerr << "Async read error: " << error.message() << std::endl;
        }
    };

    boost::asio::async_read(*backend.sockets[0], boost::asio::buffer(headerBuffer, sizeof(Protocol_Header)),
        boost::asio::bind_executor(executor, handler));
    
    Protocol_Header receivedHeader = *reinterpret_cast<Protocol_Header*>(headerBuffer);
    std::cout << "============ Header Info =============" << std::endl;
    std::cout << "Timestamp: " << receivedHeader.timestamp << std::endl;
    std::cout << "Message Type: " << receivedHeader.messageType << std::endl;
    std::cout << "Sequence Number: " << receivedHeader.sequenceNumber << std::endl;
    std::cout << "Body Length: " << receivedHeader.bodyLength << std::endl;
    std::cout << "=======================================" << std::endl;
    
    char* bodyBuffer = new char[receivedHeader.bodyLength];
    offset = 0;
    memset(bodyBuffer, 0, receivedHeader.bodyLength);
    boost::asio::async_read(*backend.sockets[0], boost::asio::buffer(bodyBuffer, receivedHeader.bodyLength),
        boost::asio::bind_executor(executor, handler));

    std::cout << "============ Body Info =============" << std::endl;
    std::cout << "Body: " << bodyBuffer << std::endl;
    std::cout << "====================================" << std::endl;

    return receivedHeader;
}

bool TcpClient::setDataRequestMessage(stDataRequestMsg& msg, uint8_t messageType) {
    msg.header = setHeader(messageType);
    msg.mRequestStatus = 0;
    msg.mDataType = 1;
    msg.mSensorChannel = 524288;
    msg.mServiceID = 0;
    msg.mNetworkID = 0;

    return true;
}

bool TcpClient::sendDataRequestMessage(Backend& backend, int idx) {
    Header header = setHeader(MessageType::DATA_SEND_REQUEST);
    stDataRequestMsg msg;
    setDataRequestMessage(msg, MessageType::DATA_SEND_REQUEST);
    char* headerBuffer = new char[21];
    int offset = 0;
    auto executor = backend.sockets[0]->get_executor();
    auto handler = [this, idx](const boost::system::error_code& error, std::size_t bytes_transferred) {
        if (error) {
            std::cerr << "Async write error: " << error.message() << std::endl;
        }
    };

    header.bodyLength = sizeof(msg);

    memcpy(headerBuffer + offset, &header.timestamp, sizeof(header.timestamp));
    offset += sizeof(header.timestamp);
    memcpy(headerBuffer + offset, &header.messageType, sizeof(header.messageType));
    offset += sizeof(header.messageType);
    memcpy(headerBuffer + offset, &header.sequenceNumber, sizeof(header.sequenceNumber));
    offset += sizeof(header.sequenceNumber);
    memcpy(headerBuffer + offset, &header.bodyLength, sizeof(header.bodyLength));
    offset += sizeof(header.bodyLength);
    if (backend.ready && backend.sockets[0]->is_open()) {
        auto& socket = backend.sockets[0];
        boost::asio::async_write(*socket, boost::asio::buffer(headerBuffer, 21),
            boost::asio::bind_executor(executor, handler));
    }

    char* bodyBuffer = new char[header.bodyLength];
    offset = 0;
    memcpy(bodyBuffer + offset, &msg.mRequestStatus, sizeof(msg.mRequestStatus));
    offset += sizeof(msg.mRequestStatus);
    memcpy(bodyBuffer + offset, &msg.mDataType, sizeof(msg.mDataType));
    offset += sizeof(msg.mDataType);
    memcpy(bodyBuffer + offset, &msg.mSensorChannel, sizeof(msg.mSensorChannel));
    offset += sizeof(msg.mSensorChannel);
    memcpy(bodyBuffer + offset, &msg.mServiceID, sizeof(msg.mServiceID));
    offset += sizeof(msg.mServiceID);
    memcpy(bodyBuffer + offset, &msg.mNetworkID, sizeof(msg.mNetworkID));
    offset += sizeof(msg.mNetworkID);
    
    if (backend.ready && backend.sockets[0]->is_open()) {
        auto& socket = backend.sockets[0];
        boost::asio::async_write(*socket, boost::asio::buffer(bodyBuffer, header.bodyLength),
            boost::asio::bind_executor(executor, handler));
    }

    delete[] headerBuffer;
    delete[] bodyBuffer;
    return true;
}

bool TcpClient::setRecordConfigMessage(stDataRecordConfigMsg& msg, uint8_t messageType) {
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

bool TcpClient::sendLoggingMessage(uint8_t messageType, Backend& backend, int idx) {
    std::ostringstream archive_stream;
    boost::archive::text_oarchive archive(archive_stream);
    int socketIdx = 1;
    stDataRecordConfigMsg msg;
    if (!setRecordConfigMessage(msg, messageType)) {
        return false;
    }
    archive << msg;
    socketIdx = 1;
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
            auto executor = socket->get_executor();
            auto handler = [this, &backend, idx](const boost::system::error_code& error, std::size_t bytes_transferred) {
                if (error) {
                    std::cerr << "Async write error: " << error.message() << std::endl;
                } else {
                    std::cout << "Async write completed: " << bytes_transferred << " bytes" << std::endl;
                }
            };

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
        return false;
    }
    else {
        std::cerr << "Backend is not ready" << std::endl;
        return false;
    }
    return true;
} 