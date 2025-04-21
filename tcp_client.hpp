#pragma once

#include <vector>
#include <memory>
#include <boost/asio.hpp>
#include "messages.hpp"

class TcpClient {
public:
    TcpClient();
    ~TcpClient();

    struct Backend {
        std::string host;
        std::array<uint16_t, 2> ports;
        std::string name;
        bool ready;
        std::array<std::shared_ptr<boost::asio::ip::tcp::socket>, 2> sockets;
    };

    void initializeBackends();
    void cleanupSockets();
    bool connectToServer();
    bool sendLoggingMessage(uint8_t messageType, Backend& backend, int idx);
    bool sendDataRequestMessage(Backend& backend, int idx);
    Protocol_Header getReceivedHeader(Backend& backend, int idx);

    std::vector<Backend>& getBackends() { return backends; }
    std::shared_ptr<boost::asio::io_context> getIoContext() { return io_context; }

private:
    Header setHeader(uint8_t messageType);
    void writeHeader(Backend& backend, MessageType msgType);
    void parseHeader(char* headerBuffer, Header& header);
    bool setDataRequestMessage(stDataRequestMsg& msg, uint8_t messageType);
    bool setRecordConfigMessage(stDataRecordConfigMsg& msg, uint8_t messageType);

    std::vector<Backend> backends;
    std::shared_ptr<boost::asio::io_context> io_context;
    uint32_t messageCounter;
}; 