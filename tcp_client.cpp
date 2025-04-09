#include "tcp_client.hpp"
#include <boost/asio.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <sstream>
#include <chrono>
#include <iostream>

using boost::asio::ip::tcp;

namespace {
    boost::asio::ip::address to_address(const std::string& host) {
        return boost::asio::ip::make_address(host);
    }
}

TcpClient::TcpClient() 
    : io_context(std::make_shared<boost::asio::io_context>())
    , messageCounter(0) {
}

TcpClient::~TcpClient() {
}

void TcpClient::setCallbacks(ConnectionCallback onConnection, MessageCallback onMessage) {
    onConnectionCallback = std::move(onConnection);
    onMessageCallback = std::move(onMessage);
}

bool TcpClient::connect(Backend& backend) {
    bool success = true;
    for (int i = 0; i < 2; ++i) {
        if (!establishConnection(backend, i)) {
            success = false;
        }
    }
    return success;
}

void TcpClient::disconnect(Backend& backend) {
    for (auto& socket : backend.sockets) {
        if (socket && socket->is_open()) {
            boost::system::error_code ec;
            socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            socket->close(ec);
        }
        socket.reset();
    }
    backend.ready = false;
}

bool TcpClient::establishConnection(Backend& backend, int idx) {
    try {
        if (!backend.sockets[idx] || !backend.sockets[idx]->is_open()) {
            backend.sockets[idx] = std::make_shared<tcp::socket>(*io_context);
            
            tcp::endpoint endpoint(to_address(backend.host), backend.ports[idx]);
            backend.sockets[idx]->connect(endpoint);
            
            if (onConnectionCallback) {
                onConnectionCallback(backend, true);
            }
            return true;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Connection error: " << e.what() << std::endl;
        if (onConnectionCallback) {
            onConnectionCallback(backend, false);
        }
    }
    return false;
}

bool TcpClient::setMessage(stDataRecordConfigMsg& msg, uint8_t messageType) {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    
    msg.header.messageType = messageType;
    msg.header.sequenceNumber = ++messageCounter;
    msg.header.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    msg.header.bodyLength = sizeof(msg);
    
    // Set default values for logging configuration
    msg.loggingDirectoryPath = "/home/nvidia/Work/data/logging";
    msg.loggingMode = 0;
    msg.historyTime = 1;
    msg.followTime = 1;
    msg.splitTime = 1;
    msg.dataLength = 1;
    
    // Add default logging file
    stLoggingFile loggingFile;
    loggingFile.id = 0;
    loggingFile.enable = "true";
    loggingFile.namePrefix = "logging";
    loggingFile.nameSubfix = "";
    loggingFile.extension = ".log";
    msg.loggingFileList.push_back(loggingFile);
    
    // Set metadata
    stMetaData metaData;
    metaData.data["control_app"] = "control_app";
    metaData.issue = "control_app";
    msg.metaData = metaData;
    
    return true;
}

bool TcpClient::sendMessage(uint8_t messageType, Backend& backend, int idx) {
    if (!backend.sockets[idx] || !backend.sockets[idx]->is_open()) {
        return false;
    }

    try {
        stDataRecordConfigMsg msg;
        if (!setMessage(msg, messageType)) {
            return false;
        }

        std::ostringstream archive_stream;
        boost::archive::text_oarchive archive(archive_stream);
        archive << msg;
        std::string outbound_data = archive_stream.str();

        // Format the header
        std::ostringstream header_stream;
        header_stream << std::setw(header_length) << std::hex << outbound_data.size();
        if (!header_stream || header_stream.str().size() != header_length) {
            return false;
        }
        std::string outbound_header = header_stream.str();

        // Write the header and the data
        std::vector<boost::asio::const_buffer> buffers;
        buffers.push_back(boost::asio::buffer(outbound_header));
        buffers.push_back(boost::asio::buffer(outbound_data));

        boost::system::error_code error;
        boost::asio::write(*backend.sockets[idx], buffers, error);

        if (!error) {
            if (onMessageCallback) {
                onMessageCallback(backend, true);
            }
            return true;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Send error: " << e.what() << std::endl;
        if (onMessageCallback) {
            onMessageCallback(backend, false);
        }
    }
    return false;
}

void TcpClient::checkConnections(std::vector<Backend>& backends) {
    for (auto& backend : backends) {
        connect(backend);
    }
} 