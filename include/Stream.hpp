#pragma once
#include "RtmpSession.hpp"

namespace ms777 {
class Stream : public std::enable_shared_from_this<Stream>
{
public:
    Stream(std::string_view app, std::string_view name);
    ~Stream();

    void stop();
    void stop(std::shared_ptr<RtmpSession> c);

    bool publish(std::shared_ptr<RtmpSession> c);
    void subscribe(std::shared_ptr<RtmpSession> c);

    void onAudio(RtmpMessage *m);
    void onVideo(RtmpMessage *m);
    bool onMeta(std::string_view metaData);
    bool onText(uint32_t timestamp, std::string_view textData);

private:
    bool isCodecHeader(RtmpMessage *m);
    void dumpAudioFormat(RtmpMessage *m);
    void dumpVideoFormat(RtmpMessage *m, uint8_t &frameType);

private:
    std::string app_;
    std::string name_;
    std::shared_ptr<RtmpSession> pub_;
    SpIntrusiveList<RtmpSession> subs_;
    Buffer metaData_;
    Buffer audioHeader_;
    Buffer videoHeader_;
};
}
