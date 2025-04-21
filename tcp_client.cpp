#include "tcp_client.hpp"
#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <iostream>
#include <thread>
#include <unistd.h>

using boost::asio::ip::tcp;
using boost::system::error_code;

namespace {
    // Helper function to convert string to boost::asio::ip::address
    boost::asio::ip::address to_address(const std::string& host) {
        return boost::asio::ip::make_address(host);
    }
}

TcpClient::TcpClient(QObject* parent) : QObject(parent), io_context(std::make_shared<boost::asio::io_context>()) {
    // IO 컨텍스트를 별도의 스레드에서 실행
    io_thread = std::make_unique<std::thread>([this]() {
        try {
            io_context->run();
        } catch (const std::exception& e) {
            std::cerr << "IO context error: " << e.what() << std::endl;
        }
    });
}

TcpClient::~TcpClient() {
    // IO 컨텍스트와 스레드 정리
    if (io_context) {
        io_context->stop();
    }
    if (io_thread && io_thread->joinable()) {
        io_thread->join();
    }
}

void TcpClient::connectToServer(Backend& backend, const std::function<void(bool)>& callback) {
    try {
        // 소켓 생성
        backend.sockets[0] = std::make_shared<tcp::socket>(*io_context);
        
        // 엔드포인트 생성
        tcp::endpoint endpoint(to_address(backend.host), backend.ports[0]);
        
        // 비동기 연결 시작
        auto socket = backend.sockets[0];
        auto backend_ptr = std::make_shared<Backend>(backend);
        
        socket->async_connect(endpoint,
            [this, socket, backend_ptr, callback](const boost::system::error_code& error) {
                if (!error) {
                    backend_ptr->ready = true;
                    backend_ptr->sockets[0] = socket;

                    // 두 번째 소켓 연결
                    try {
                        backend_ptr->sockets[1] = std::make_shared<tcp::socket>(*io_context);
                        tcp::endpoint endpoint2(to_address(backend_ptr->host), backend_ptr->ports[1]);
                        auto socket2 = backend_ptr->sockets[1];
                        
                        socket2->async_connect(endpoint2,
                            [this, socket2, backend_ptr](const boost::system::error_code& error) {
                                if (!error) {
                                    backend_ptr->sockets[1] = socket2;
                                } else {
                                    std::cerr << "Error connecting to second port: " << error.message() << std::endl;
                                }
                            });
                    } catch (const std::exception& e) {
                        std::cerr << "Error setting up second socket: " << e.what() << std::endl;
                    }
                    callback(true);
                } else {
                    std::cerr << "Error with " << backend_ptr->name << ":" 
                            << backend_ptr->ports[0] << ": " << error.message() << std::endl;
                    callback(false);
                }
            });
    } catch (const std::exception& e) {
        std::cerr << "Error setting up socket: " << e.what() << std::endl;
        callback(false);
    }
}

void TcpClient::cleanupSockets() {
    // 모든 백엔드의 소켓을 정리
    io_context->stop();
    if (io_thread && io_thread->joinable()) {
        io_thread->join();
    }
    io_context = std::make_shared<boost::asio::io_context>();
    io_thread = std::make_unique<std::thread>([this]() {
        try {
            io_context->run();
        } catch (const std::exception& e) {
            std::cerr << "IO context error: " << e.what() << std::endl;
        }
    });
}

Header TcpClient::setHeader(uint8_t messageType, uint32_t& messageCounter) {
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

void TcpClient::writeHeader(Backend& backend, MessageType msgType, uint32_t& messageCounter) {
    char* headerBuffer = new char[22];
    Header sendHeader = setHeader(msgType, messageCounter);
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
    delete[] headerBuffer;
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

    delete[] headerBuffer;
    delete[] bodyBuffer;
    return receivedHeader;
}

bool TcpClient::setDataRequestMessage(stDataRequestMsg& msg, uint8_t messageType, uint32_t& messageCounter) {
    msg.header = setHeader(messageType, messageCounter);
    msg.mRequestStatus = 0;
    msg.mDataType = 1;
    msg.mSensorChannel = 524288;
    msg.mServiceID = 0;
    msg.mNetworkID = 0;

    return true;
}

bool TcpClient::sendDataRequestMessage(Backend& backend, int idx, uint32_t& messageCounter) {
    Header header = setHeader(MessageType::DATA_SEND_REQUEST, messageCounter);
    stDataRequestMsg msg;
    setDataRequestMessage(msg, MessageType::DATA_SEND_REQUEST, messageCounter);
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

bool TcpClient::setRecordConfigMessage(stDataRecordConfigMsg& msg, uint8_t messageType, uint32_t& messageCounter) {
    msg.header = setHeader(messageType, messageCounter);

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
    // 메시지 전송 로직
    if (backend.ready && backend.sockets[0] && backend.sockets[0]->is_open()) {
        try {
            // 메시지 전송 구현
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error sending message: " << e.what() << std::endl;
            return false;
        }
    }
    return false;
} 