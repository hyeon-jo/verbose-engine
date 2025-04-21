#pragma once

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <memory>
#include <functional>
#include <QObject>
#include <opencv2/opencv.hpp>
#include "messages.hpp"

class TcpClient : public QObject {
    Q_OBJECT

public:
    explicit TcpClient(QObject* parent = nullptr);
    ~TcpClient();

    void connectToServer(Backend& backend, const std::function<void(bool)>& callback);
    void cleanupSockets();
    Header setHeader(uint8_t messageType, uint32_t& messageCounter);
    void writeHeader(Backend& backend, MessageType msgType, uint32_t& messageCounter);
    void parseHeader(char* headerBuffer, Header& header);
    Protocol_Header getReceivedHeader(Backend& backend, int idx);
    bool setDataRequestMessage(stDataRequestMsg& msg, uint8_t messageType, uint32_t& messageCounter);
    bool sendDataRequestMessage(Backend& backend, int idx, uint32_t& messageCounter);
    bool setRecordConfigMessage(stDataRecordConfigMsg& msg, uint8_t messageType, uint32_t& messageCounter);
    bool sendLoggingMessage(uint8_t messageType, Backend& backend, int idx);

signals:
    void imageReceived(int frameIndex, const cv::Mat& image);

private:
    std::shared_ptr<boost::asio::io_context> io_context;
    std::unique_ptr<std::thread> io_thread;
}; 