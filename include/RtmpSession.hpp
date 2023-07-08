#pragma once
#include <boost/asio.hpp>
#include <string>
#include "Buffer.hpp"
#include "Intrusive.hpp"

namespace ms777 {
constexpr std::size_t RTMP_MAX_CHANNELS = 8;
constexpr uint32_t RTMP_DEFAULT_CHUNK_SIZE = 128;

class RtmpServer;
class Stream;

struct RtmpMessageHeader {
    uint8_t type{ 0 };
    uint32_t cid{ 0 };
    uint32_t timestamp;
    uint32_t clock;
    uint32_t sid;
    uint32_t length{ 0 };
};

struct RtmpMessage {
    RtmpMessageHeader h;
    Buffer payload;
};

class RtmpSession
    : public std::enable_shared_from_this<RtmpSession>
    , public SpIntrusiveList<RtmpSession>::Hook
{
public:
    RtmpSession(RtmpServer &server, boost::asio::ip::tcp::socket socket);

    void start();
    void stop();

    enum class Type {
        HOST, CLIENT
    };

    enum class Direction {
        NONE, INPUT, OUTPUT
    };

    Direction direction()
    {
        return dir_;
    }

    void setStream(std::shared_ptr<Stream> stream);

    std::string &app()
    {
        return app_;
    }

    std::string &name()
    {
        return name_;
    }

    void sendAudioHeader(Buffer *audio);
    void sendAudio(uint32_t timestamp, std::string_view payload);
    void sendVideoHeader(Buffer *video);
    void sendVideo(uint32_t timestamp, std::string_view payload);
    void sendMetaData(std::string_view metaData);

private:
    void doReadC0C1();
    void doWriteC0C1();
    void doReadS0S1();
    void doReadC2S2();
    void doReadChunk();
    bool parseC0C1GenerateS0S1S2();
    bool parseS0S1GenerateC2();
    bool onMessage(RtmpMessage *m);
    bool onInvoke(RtmpMessage *m);
    bool onNotify(RtmpMessage *m);
    void doWrite();
    bool decodeChunkHeader();
    bool decodeChunkPayload();
    void stopSession();
    RtmpMessage *getMessage(uint32_t cid);

private:
    RtmpServer &server_;
    boost::asio::ip::tcp::socket socket_;
    Type type_;
    Direction dir_;
    Buffer inBuffer_;
    Buffer outBuffer_;
    Buffer outBufferFlush_;
    uint32_t inChunkSize_{ RTMP_DEFAULT_CHUNK_SIZE };
    uint32_t outChunkSize_{ RTMP_DEFAULT_CHUNK_SIZE };
    RtmpMessage inMessages_[RTMP_MAX_CHANNELS];
    bool readingChunkHeader_{ true };
    uint8_t chunkHeaderFmt_{ 0 };
    uint32_t chunkHeaderCid_{ 0 };
    bool writing_{ false };
    std::shared_ptr<Stream> stream_;
    std::string app_;
    std::string name_;
};
}
