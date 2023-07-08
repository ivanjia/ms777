#include <spdlog/spdlog.h>
#include "Stream.hpp"

namespace ms777 {
Stream::Stream(std::string_view app, std::string_view name)
    : app_(app), name_(name)
{
    SPDLOG_INFO("Stream {} created for {}/{}", (void *)this, app, name);
}

Stream::~Stream()
{
    SPDLOG_INFO("Stream {} is closed", (void *)this);
}

void Stream::stop()
{
    SPDLOG_INFO("Stream {}, stop all sessions", (void *)this);
    if(pub_) {
        pub_->stop();
        pub_.reset();
    }
    std::shared_ptr<RtmpSession> c = subs_.front();
    while(c) {
        c->stop();
        c = SpIntrusiveList<RtmpSession>::next(c);
    }
    subs_.clear();
}

void Stream::stop(std::shared_ptr<RtmpSession> c)
{
    if(c->direction() == RtmpSession::Direction::INPUT) {
        SPDLOG_INFO("Stream {}, stop session {}, which is pub", (void *)this, (void *)c.get());
        assert(c->get() == pub_.get());
        pub_.reset();
        c->stop();
    } else {
        SPDLOG_INFO("Stream {}, stop session {}, which is sub", (void *)this, (void *)c.get());
        subs_.erase(c);
        c->stop();
    }
}

bool Stream::publish(std::shared_ptr<RtmpSession> c)
{
    if(pub_) {
        SPDLOG_ERROR("Stream {}, already published, reject {}", (void *)this, (void *)c.get());
        return false;
    }
    SPDLOG_INFO("Stream {}, published by {}", (void *)this, (void *)c.get());
    pub_ = c;
    return true;
}

void Stream::subscribe(std::shared_ptr<RtmpSession> c)
{
    SPDLOG_INFO("Stream {}, added sub {}", (void *)this, (void *)c.get());
    subs_.addFront(c);
    if(audioHeader_.readableSize() > 0) {
        c->sendAudioHeader(&audioHeader_);
    }
    if(videoHeader_.readableSize() > 0) {
        c->sendVideoHeader(&videoHeader_);
    }
}

bool Stream::isCodecHeader(RtmpMessage *m)
{
    if(m->payload.readableSize() >= 2) {
        if(*(m->payload.readBuffer() + 1) == 0) {
            return true;
        }
    }
    return false;
}

void Stream::dumpAudioFormat(RtmpMessage *m)
{
    uint8_t format = *m->payload.readBuffer();
    uint8_t codec = ((format & 0xf0) >> 4);
    uint32_t channels = (format & 0x01) + 1;
    uint32_t sampleSize = (format & 0x02) ? 2 : 1;
    SPDLOG_DEBUG("Stream {}, audio codec {}, channels {}, sampleSize {}", (void *)this, codec, channels, sampleSize);
}

void Stream::onAudio(RtmpMessage *m)
{
    if(isCodecHeader(m)) {
        dumpAudioFormat(m);
        audioHeader_.clear();
        audioHeader_.append(m->payload.readBuffer(), m->payload.readableSize());
        auto c = subs_.front();
        while(c) {
            c->sendAudioHeader(&audioHeader_);
            c = SpIntrusiveList<RtmpSession>::next(c);
        }
    } else {
        auto c = subs_.front();
        while(c) {
            c->sendAudio(m->h.clock, m->payload.stringView());
            c = SpIntrusiveList<RtmpSession>::next(c);
        }
    }
}

void Stream::dumpVideoFormat(RtmpMessage *m, uint8_t &frameType)
{
    uint8_t format = *m->payload.readBuffer();
    frameType = (format & 0xf0) >> 4;
    uint8_t codec = format & 0x0f;
    SPDLOG_DEBUG("Stream {}, video codec {}, type {}", (void *)this, codec, frameType);
}

void Stream::onVideo(RtmpMessage *m)
{
    if(isCodecHeader(m)) {
        uint8_t frameType;
        dumpVideoFormat(m, frameType);
        if(frameType == 1) {
            // KEY FRAME
            videoHeader_.clear();
            videoHeader_.append(m->payload.readBuffer(), m->payload.readableSize());
            auto c = subs_.front();
            while(c) {
                c->sendVideoHeader(&videoHeader_);
                c = SpIntrusiveList<RtmpSession>::next(c);
            }
        }
    } else {
        auto c = subs_.front();
        while(c) {
            c->sendVideo(m->h.clock, m->payload.stringView());
            c = SpIntrusiveList<RtmpSession>::next(c);
        }
    }
}

bool Stream::onMeta(std::string_view metaData)
{
    metaData_.clear();
    metaData_.append(metaData);
    auto c = subs_.front();
    while(c) {
        c->sendMetaData(metaData_.stringView());
        c = SpIntrusiveList<RtmpSession>::next(c);
    }
    return true;
}

bool Stream::onText(uint32_t timestamp, std::string_view textData)
{
    return true;
}
}
