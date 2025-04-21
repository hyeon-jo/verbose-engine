#pragma once

#include <vector>
#include <string>
#include <array>
#include <memory>
#include <map>
#include <boost/asio.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/serialization/base_object.hpp>
#include <thread>

enum { header_length = 8 };

enum MessageType : uint8_t {
    LINK = 1,
    LINK_ACK = 2,
    REC_INFO = 3,
    REC_INFO_ACK = 4,
    DATA_SENSOR = 5,
    DATA_SENSOR_ACK = 6,
    DATA_SEND_REQUEST = 17,
    START = 19,
    EVENT = 20,
    STOP = 24,
    CONFIG_INFO = 26,
};

enum eDataType
{
    SENSOR = 1,
    RECOGNITION = 2,
    LIDAR = 3,
    LIDAR_RAW = 4,
    RESOURCE_INFO = 5,
    DEBUG_MESSAGE = 6,
    RECONGITION_RESULT = 7,
    MAX_DATA_TYPE = 8
};

enum class eSensorChannel:uint8_t
{
    CAMERA_FRONT = 0,
    CAMERA_FRONT_SIDE_LEFT = 1,
    CAMERA_FRONT_SIDE_RIGHT = 2,
    CAMERA_FRONT_TELE = 3,
    CAMERA_REAR = 4,
    CAMERA_REAR_SIDE_LEFT = 5,
    CAMERA_REAR_SIDE_RIGHT = 6,
    CAMERA_SR_FRONT = 7,
    CAMERA_SR_LEFT = 8,
    CAMERA_SR_RIGHT = 9,
    CAMERA_SR_REAR = 10,
    LIDAR_FRONT_CENTER = 11,
    LIDAR_FRONT_LEFT = 12,
    LIDAR_FRONT_RIGHT = 13,
    LIDAR_REAR_CENTER = 14,
    LIDAR_REAR_LEFT = 15,
    LIDAR_REAR_RIGHT = 16,
    LIDAR_SIDE_LEFT = 17,
    LIDAR_SIDE_RIGHT = 18,
    LIDAR_ROOF_CENTER = 19,
    LIDAR_ROOF_FRONT = 20,
    LIDAR_ROOF_LEFT = 21,
    LIDAR_ROOF_RIGHT = 22,
    LIDAR_ROOF_REAR = 23,
    WEBCAM_FRONT = 24,
    WEBCAM_FRONT_SIDE_LEFT = 25,
    WEBCAM_FRONT_SIDE_RIGHT = 26,
    WEBCAM_REAR = 27,
    WEBCAM_REAR_SIDE_LEFT = 28,
    WEBCAM_REAR_SIDE_RIGHT = 29,
    WEBCAM_SIDE_LEFT = 30,
    WEBCAM_SIDE_RIGHT = 31,
    CHANNEL_MAX = 32
};

inline uint32_t getSensorChannelBitmask(eSensorChannel channel) {
    return 0x01 << static_cast<uint32_t>(channel);
}

class [[gnu::packed]] Protocol_Header {
public:
    Protocol_Header();

    //clang-format off
    Protocol_Header(
        uint8_t const messageType,
        uint64_t const sequenceNumber,
        uint32_t const bodyLength
    );
    //clang-format on

    ~Protocol_Header() = default;

    uint64_t timestamp;
    uint8_t messageType;
    uint64_t sequenceNumber;
    uint32_t bodyLength;
    uint8_t mResult;
};

struct Header
{
    uint64_t timestamp;
    uint8_t messageType;
    uint64_t sequenceNumber;
    uint32_t bodyLength;

    template <typename Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar & timestamp;
        ar & messageType;
        ar & sequenceNumber;
        ar & bodyLength;
    }
};

struct stLoggingFile
{
    uint32_t id;
    std::string enable;
    std::string namePrefix;
    std::string nameSubfix;
    std::string extension;
    
    template <typename Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar & id;
        ar & enable;
        ar & namePrefix;
        ar & nameSubfix;
        ar & extension;
    }
};

struct stMetaData
{
    std::map<std::string, std::string> data;
    std::string issue;
    
    template <typename Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar & data;
        ar & issue;
    }
};

struct stDataRecordConfigMsg
{
    Header header;
    std::string loggingDirectoryPath;
    uint32_t loggingMode;
    uint32_t historyTime;
    uint32_t followTime;
    uint32_t splitTime;
    uint32_t dataLength;
    std::vector<stLoggingFile> loggingFileList;
    stMetaData metaData;

    template <typename Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar & header;
        ar & loggingDirectoryPath;
        ar & loggingMode;
        ar & historyTime;
        ar & followTime;
        ar & splitTime;
        ar & dataLength;
        ar & loggingFileList;
        ar & metaData;
    }
};

struct stDataRequestMsg
{
    Header header;
    uint8_t mRequestStatus;
    uint8_t mDataType;
    uint32_t mSensorChannel;
    uint8_t mServiceID;
    uint8_t mNetworkID;
    
    template <typename Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar & header;
        ar & mRequestStatus;
        ar & mDataType;
        ar & mSensorChannel;
        ar & mServiceID;
        ar & mNetworkID;
    }
};

struct stDataSensorReqMsg
{
    // uint32_t mSequenceNumber;
    // uint32_t mTotalNumber;
    uint32_t mCurrentNumber;
    uint32_t mFrameNumber;
    uint64_t mTimestamp;
    uint8_t mSensorType;
    uint8_t mChannel;
    uint16_t mImgWidth;
    uint16_t mImgHeight;
    uint8_t mImgDepth;
    uint8_t mImgFormat;
    uint32_t mNumPoints;
    uint32_t mPayloadSize;
};