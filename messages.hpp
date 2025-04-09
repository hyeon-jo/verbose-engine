#ifndef MESSAGES_HPP
#define MESSAGES_HPP

#include <vector>
#include <map>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/serialization/base_object.hpp>
#include <thread>


enum eDataType
{
    SENSOR = 1,
    RECOGNITION_SERVICE,
    LIDAR_SERVICE,
    LIDAR_RAW_SERVICE,
    RESOURCE_INFO,
    DEBUG_MESSAGE,
    RECOGNITION_RESULT,
    MAX_DATA_TYPE
};

enum class eSensorChannel:uint8_t
{
    SENSOR_CH_CAMERA_FRONT,
    SENSOR_CH_CAMERA_FRONT_SIDE_LEFT,
    SENSOR_CH_CAMERA_FRONT_SIDE_RIGHT,
    SENSOR_CH_CAMERA_FRONT_TELE,
    SENSOR_CH_CAMERA_REAR,
    SENSOR_CH_CAMERA_REAR_SIDE_LEFT,
    SENSOR_CH_CAMERA_REAR_SIDE_RIGHT,
    SENSOR_CH_CAMERA_SR_FRONT,
    SENSOR_CH_CAMERA_SR_LEFT,
    SENSOR_CH_CAMERA_SR_RIGHT,
    SENSOR_CH_CAMERA_SR_REAR,
    SENSOR_CH_LIDAR_FRONT_CENTOR,
    SENSOR_CH_LIDAR_FRONT_LEFT,
    SENSOR_CH_LIDAR_FRONT_RIGHT,
    SENSOR_CH_LIDAR_REAR_CENTER,
    SENSOR_CH_LIDAR_REAR_LEFT,
    SENSOR_CH_LIDAR_REAR_RIGHT,
    SENSOR_CH_LIDAR_SIDE_LEFT,
    SENSOR_CH_LIDAR_SIDE_RIGHT,
    SENSOR_CH_LIDAR_ROOF_CENTER,
    SENSOR_CH_LIDAR_ROOF_FRONT,
    SENSOR_CH_LIDAR_ROOF_LEFT,
    SENSOR_CH_LIDAR_ROOF_RIGHT,
    SENSOR_CH_LIDAR_ROOF_REAR,
    SENSOR_CH_WEBCAM_FRONT,
    SENSOR_CH_WEBCAM_FRONT_SIDE_LEFT,
    SENSOR_CH_WEBCAM_FRONT_SIDE_RIGHT,
    SENSOR_CH_WEBCAM_REAR,
    SENSOR_CH_WEBCAM_REAR_SIDE_LEFT,
    SENSOR_CH_WEBCAM_REAR_SIDE_RIGHT,
    SENSOR_CH_WEBCAM_SIDE_LEFT,
    SENSOR_CH_WEBCAM_SIDE_RIGHT,
    SENSOR_CH_MAX

};


// uint32_t getSensorBitmask(eSensorChannel channel)
// {
//     return 0x1 << static_cast<uint8_t>(channel);
// }

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

struct stDataRecordViewerMsg
{
    Header header;
    std::string registerNum;
    std::string issueLog;
    std::string controlId;

    template <typename Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar & header;
        ar & registerNum;
        ar & issueLog;
        ar & controlId;
    }
};

#endif // MESSAGES_HPP