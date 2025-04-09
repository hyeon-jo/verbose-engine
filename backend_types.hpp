#pragma once

#include <string>
#include <array>
#include <memory>
#include <boost/asio.hpp>

enum { header_length = 8 };

enum MessageType : uint8_t {
    start = 19,
    event = 20,
    stop = 24,
    configInfo = 26,
};

struct Backend {
    std::string host;
    std::array<int, 2> ports;
    std::string name;
    bool ready;
    std::array<std::shared_ptr<boost::asio::ip::tcp::socket>, 2> sockets;
}; 