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