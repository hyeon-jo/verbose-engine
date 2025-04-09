#pragma once

#include <memory>
#include <functional>
#include "backend_types.hpp"
#include "messages.hpp"

class TcpClient {
public:
    using ConnectionCallback = std::function<void(const Backend&, bool)>;
    using MessageCallback = std::function<void(const Backend&, bool)>;

    TcpClient();
    ~TcpClient();

    void setCallbacks(ConnectionCallback onConnection, MessageCallback onMessage);
    bool connect(Backend& backend);
    void disconnect(Backend& backend);
    bool sendMessage(uint8_t messageType, Backend& backend, int idx);
    void checkConnections(std::vector<Backend>& backends);

private:
    bool setMessage(stDataRecordConfigMsg& msg, uint8_t messageType);
    bool establishConnection(Backend& backend, int idx);

    std::shared_ptr<boost::asio::io_context> io_context;
    ConnectionCallback onConnectionCallback;
    MessageCallback onMessageCallback;
    uint32_t messageCounter;
}; 