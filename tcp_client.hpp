#pragma once

#include <vector>
#include <memory>
#include <boost/asio.hpp>
#include "messages.hpp"
#include "control_app.hpp"

class ControlApp;
struct Backend;

class TcpClient {
public:
    TcpClient(ControlApp* app = nullptr);
    ~TcpClient();

    void initializeBackends();
    void cleanupSockets();
    bool connectToServer();
    bool sendLoggingMessage(uint8_t messageType, Backend& backend, int idx);
    bool sendDataRequestMessage(Backend& backend, int idx);
    Protocol_Header getReceivedHeader(Backend& backend, int idx);
    void sendData();
    void receiveData();
    std::vector<Backend>& getBackends() { return backends; }
    std::shared_ptr<boost::asio::io_context> getIoContext() { return io_context; }

private:
    Header setHeader(uint8_t messageType);
    void writeHeader(Backend& backend, MessageType msgType);
    void parseHeader(char* headerBuffer, Header& header);
    bool setDataRequestMessage(stDataRequestMsg& msg, uint8_t messageType);
    bool setRecordConfigMessage(stDataRecordConfigMsg& msg, uint8_t messageType);
    std::vector<char*> receiveAll(Backend& backend, const size_t bufferSize, size_t recvSize);

    std::vector<Backend> backends;
    std::shared_ptr<boost::asio::io_context> io_context;
    uint32_t messageCounter;
    int sendStatus = 0;
    ControlApp* controlApp;
}; 