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

TcpClient::TcpClient(ControlApp* app) : messageCounter(0),
    io_context(std::make_shared<boost::asio::io_context>()),
    controlApp(app) {
    initializeBackends();
}

TcpClient::~TcpClient() {
    cleanupSockets();
}

void TcpClient::initializeBackends() {
    backends.clear();
    backends.push_back(Backend{
        .host = "127.0.0.1",
        .ports = {9090, 9091},
        .name = "Backend 1",
        .ready = false,
        .sockets = {}
    });
    backends.push_back(Backend{
        .host = "192.168.10.20",
        .ports = {9090, 9091},
        .name = "Backend 2",
        .ready = false,
        .sockets = {}
    });
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
    header.sequenceNumber = messageCounter++;
    header.bodyLength = 1;

    if (messageType == MessageType::DATA_SEND_REQUEST) {
        // messageCounter++;
        header.bodyLength = 8;
    }

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
    // usleep(10000);
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
    std::cout << "Message Type: " << static_cast<int>(receivedHeader.messageType) << std::endl;
    std::cout << "Sequence Number: " << receivedHeader.sequenceNumber << std::endl;
    std::cout << "Body Length: " << receivedHeader.bodyLength << std::endl;
    std::cout << "=======================================" << std::endl;
    
    return receivedHeader;
}

char* TcpClient::receiveBody(Backend& backend, int idx, int bodyLength) {
    auto executor = backend.sockets[0]->get_executor();
    auto handler = [this](const boost::system::error_code& error, std::size_t bytes_transferred) {
        if (error) {
            std::cerr << "Async read error: " << error.message() << std::endl;
        }
    };
    if (bodyLength - 1 > 0) {
        char* bodyBuffer = new char[bodyLength - 1];
        boost::asio::async_read(*backend.sockets[0], boost::asio::buffer(bodyBuffer, bodyLength - 1),
            boost::asio::bind_executor(executor, handler));

        std::cout << "============ Body Info =============" << std::endl;
        std::cout << "Body: " << strlen(bodyBuffer) << std::endl;
        std::cout << "====================================" << std::endl;

        return bodyBuffer;
    }
    else {
        char* bodyBuffer = new char[1];
        bodyBuffer[0] = 0;
        return bodyBuffer;
    }
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
    stDataRequestMsg msg;
    setDataRequestMessage(msg, MessageType::DATA_SEND_REQUEST);
    char* headerBuffer = new char[sizeof(stDataRequestMsg)];
    int offset = 0;
    auto executor = backend.sockets[0]->get_executor();
    auto handler = [this, idx](const boost::system::error_code& error, std::size_t bytes_transferred) {
        if (error) {
            std::cerr << "Async write error: " << error.message() << std::endl;
        }
    };

    auto header = msg.header;

    memcpy(headerBuffer + offset, &header.timestamp, sizeof(header.timestamp));
    offset += sizeof(header.timestamp);
    memcpy(headerBuffer + offset, &header.messageType, sizeof(header.messageType));
    offset += sizeof(header.messageType);
    memcpy(headerBuffer + offset, &header.sequenceNumber, sizeof(header.sequenceNumber));
    offset += sizeof(header.sequenceNumber);
    memcpy(headerBuffer + offset, &header.bodyLength, sizeof(header.bodyLength));
    offset += sizeof(header.bodyLength);

    memcpy(headerBuffer + offset, &msg.mRequestStatus, sizeof(msg.mRequestStatus));
    offset += sizeof(msg.mRequestStatus);
    memcpy(headerBuffer + offset, &msg.mDataType, sizeof(msg.mDataType));
    offset += sizeof(msg.mDataType);
    memcpy(headerBuffer + offset, &msg.mSensorChannel, sizeof(msg.mSensorChannel));
    offset += sizeof(msg.mSensorChannel);
    memcpy(headerBuffer + offset, &msg.mServiceID, sizeof(msg.mServiceID));
    offset += sizeof(msg.mServiceID);
    memcpy(headerBuffer + offset, &msg.mNetworkID, sizeof(msg.mNetworkID));
    offset += sizeof(msg.mNetworkID);

    if (backend.ready && backend.sockets[0]->is_open()) {
        auto& socket = backend.sockets[0];
        boost::asio::async_write(*socket, boost::asio::buffer(headerBuffer, sizeof(stDataRequestMsg)),
            boost::asio::bind_executor(executor, handler));
    }

    delete[] headerBuffer;
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

void TcpClient::sendData() {
    // TODO: Implement
    while (true) {
        if (sendStatus == 1) {
            for (auto& backend : backends) {
                writeHeader(backend, MessageType::LINK);
                std::cout << "[SEND] LINK" << std::endl;
            }
            sendStatus = 2;
        }
        else if (sendStatus == 3) {
            for (auto& backend : backends) {
                writeHeader(backend, MessageType::REC_INFO);
                std::cout << "[SEND] REC_INFO" << std::endl;
            }
            sendStatus = 4;
        }
        else if (sendStatus == 5) {
            int idx = 0;
            for (auto& backend : backends) {
                sendDataRequestMessage(backend, idx++);
                std::cout << "[SEND] DATA_SEND_REQUEST" << std::endl;
            }
            sendStatus = 6;
        }
    }
}

std::vector<char*> TcpClient::receiveAll(Backend& backend, const size_t bufferSize, size_t recvSize) {

    auto executor = backend.sockets[0]->get_executor();
    auto handler = [this](const boost::system::error_code& error, std::size_t bytes_transferred) {
        if (error) {
            std::cerr << "Async read error: " << error.message() << std::endl;
        }
    };

    std::vector<char*> data;
    char* part = new char[bufferSize];

    while (true) {

        if (recvSize - strlen(part) < bufferSize) {
            boost::asio::async_read(*backend.sockets[0], boost::asio::buffer(part, recvSize - strlen(part) - 1),
                boost::asio::bind_executor(executor, handler));
            data.push_back(part);
            break;
        }

        boost::asio::async_read(*backend.sockets[0], boost::asio::buffer(part, bufferSize),
            boost::asio::bind_executor(executor, handler));
        data.push_back(part);

        if (strlen(part) < bufferSize) {
            break;
        }
    }

    return data;
}

void TcpClient::receiveData() {
    while (true) {
        if (sendStatus == 2) {
            sleep(1);
            int idx = 0;
            auto flag = false;
            std::cout << "[RECV] LINK_ACK" << std::endl;
            for (auto& backend : backends) {
                auto header = getReceivedHeader(backend, idx);
                auto body = receiveBody(backend, idx++, header.bodyLength);
                if (header.messageType == MessageType::LINK_ACK) {
                    if (!flag) {
                        flag = true;
                    }
                    else {
                        sendStatus = 3;
                    }
                }
            }
            if (!flag) {
                std::cerr << "Link ACK not received" << std::endl;
                // sendStatus = 1;
            }
            else
            {
                sendStatus = 3;
            }
        }
        else if (sendStatus == 4) {
            sleep(1);
            std::cout << "[RECV] REC_INFO_ACK" << std::endl;
            int offset = 7;
            int idx = 0;
            for (auto& backend : backends) {
                auto header = getReceivedHeader(backend, idx);
                auto body = receiveBody(backend, idx++, header.bodyLength);
            }
            sendStatus = 5;
        }
        else if (sendStatus == 6) {
            usleep(500000);
            std::cout << "[RECV] DATA_SENSOR" << std::endl;
            int idx = 0;
            for (auto& backend : backends) {
                auto header = getReceivedHeader(backend, idx);
                auto body = receiveBody(backend, idx++, header.bodyLength);

                if (header.messageType == MessageType::DATA_SENSOR) {
                    char* newBody = new char[header.bodyLength];
                    memcpy(newBody, header.mResult, 1);
                    memcpy(newBody + 1, body, header.bodyLength - 1);
                    stDataSensorReqMsg* dataSensorReqMsg = reinterpret_cast<stDataSensorReqMsg*>(newBody);
                    std::cout << "============ Data Sensor Request Message =============" << std::endl;
                    std::cout << "Data Sensor Request Message: " << dataSensorReqMsg->mCurrentNumber << std::endl;
                    std::cout << "Data Sensor Request Message: " << dataSensorReqMsg->mFrameNumber << std::endl;
                    std::cout << "Data Sensor Request Message: " << dataSensorReqMsg->mTimestamp << std::endl;
                    std::cout << "Data Sensor Request Message: " << dataSensorReqMsg->mSensorType << std::endl;
                    std::cout << "Data Sensor Request Message: " << dataSensorReqMsg->mChannel << std::endl;
                    std::cout << "Data Sensor Request Message: " << dataSensorReqMsg->mImgWidth << std::endl;
                    std::cout << "Data Sensor Request Message: " << dataSensorReqMsg->mImgHeight << std::endl;
                    std::cout << "Data Sensor Request Message: " << dataSensorReqMsg->mImgDepth << std::endl;
                    std::cout << "Data Sensor Request Message: " << dataSensorReqMsg->mImgFormat << std::endl;
                    std::cout << "Data Sensor Request Message: " << dataSensorReqMsg->mNumPoints << std::endl;
                    std::cout << "Data Sensor Request Message: " << dataSensorReqMsg->mPayloadSize << std::endl;
                    std::cout << "=========================================================" << std::endl;

                    if (dataSensorReqMsg->mSensorType == 1) {
                        auto image_buffer = receiveBody(backend, idx, dataSensorReqMsg->mPayloadSize);
                        controlApp->processData(image_buffer, 1, dataSensorReqMsg->mImgWidth, dataSensorReqMsg->mImgHeight);
                    }
                    else if (dataSensorReqMsg->mSensorType == 2) {
                        // TODO: Implement
                    }
                    
                    
                }
            }
        }
    }
}   