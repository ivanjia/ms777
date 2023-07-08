#include <cassert>
#include <chrono>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>
#include "RtmpServer.hpp"
#include "RtmpSession.hpp"
#include "Rtmp.hpp"
#include "Conf.hpp"

namespace ms777 {
static inline uint64_t timeNow()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>
           (std::chrono::system_clock::now().time_since_epoch()).count();
}

RtmpSession::RtmpSession(RtmpServer &server, boost::asio::ip::tcp::socket socket)
    : server_(server),
      socket_(std::move(socket)),
      type_(Type::HOST),
      dir_(Direction::NONE),
      inBuffer_(FLAGS_rtmp_read_buffer_size),
      outBuffer_(1024),
      outBufferFlush_(1024)
{
}

void RtmpSession::start()
{
    SPDLOG_INFO("RTMP session {}, wait handshake", (void *)this);
    doReadC0C1();
}

void RtmpSession::stop()
{
    SPDLOG_INFO("RTMP session {}, close socket", (void *)this);
    socket_.close();
}

void RtmpSession::setStream(std::shared_ptr<Stream> stream)
{
    stream_ = stream;
}

void RtmpSession::doReadC0C1()
{
    auto self(shared_from_this());
    inBuffer_.reserve(1 + rtmp::HANDSHAKE_SIZE);
    boost::asio::async_read(socket_, boost::asio::buffer(inBuffer_.writeBuffer(), 1 + rtmp::HANDSHAKE_SIZE),
    [this, self](const boost::system::error_code & ec, std::size_t len) {
        if(!ec) {
            inBuffer_.commit(1 + rtmp::HANDSHAKE_SIZE);
            if(!parseC0C1GenerateS0S1S2()) {
                stopSession();
            }
        }  else if(ec != boost::asio::error::operation_aborted) {
            SPDLOG_ERROR("RTMP session {} error when read handshake challenge, closing", (void *)this);
            stopSession();
        }
    });
}

bool RtmpSession::parseC0C1GenerateS0S1S2()
{
    if(*inBuffer_.readBuffer() != rtmp::HANDSHAKE_VERSION) {
        SPDLOG_ERROR("RTMP session {}, invalid handshake version {}", (void *)this, (int)*inBuffer_.readBuffer());
        return false;
    }
    uint32_t peer_epoch;
    loadBE<uint32_t, 32>(inBuffer_.readBuffer() + 1, peer_epoch);
    uint32_t ver;
    loadBE<uint32_t, 32>(inBuffer_.readBuffer() + 5, ver);
    if(ver != 0) {
        SPDLOG_ERROR("RTMP session {}, not support complex handshake, ver={}", (void *)this, ver);
        return false;
    }
    outBuffer_.reserve(1 + 2 * rtmp::HANDSHAKE_SIZE);
    *outBuffer_.writeBuffer() = rtmp::HANDSHAKE_VERSION;
    storeBE<uint32_t, 32>(outBuffer_.writeBuffer() + 1, static_cast<uint32_t>(timeNow() / 1000));
    storeBE<uint32_t, 32>(outBuffer_.writeBuffer() + 5, 0);
    outBuffer_.commit(1 + rtmp::HANDSHAKE_SIZE);
    // echo c1
    outBuffer_.append(inBuffer_.readBuffer() + 1, rtmp::HANDSHAKE_SIZE);
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(outBuffer_.readBuffer(), outBuffer_.readableSize()),
    [this, self](const boost::system::error_code & ec, std::size_t) {
        if(!ec) {
            outBuffer_.clear();
            doReadC2S2();
        } else if(ec != boost::asio::error::operation_aborted) {
            SPDLOG_ERROR("RTMP session {}, fail to send handshake response", (void *)this);
            stopSession();
        }
    });
    return true;
}

void RtmpSession::doWriteC0C1()
{
    *outBuffer_.writeBuffer() = rtmp::HANDSHAKE_VERSION;
    storeBE<uint32_t, 32>(outBuffer_.writeBuffer() + 1, static_cast<uint32_t>(timeNow() / 1000));
    storeBE<uint32_t, 32>(outBuffer_.writeBuffer() + 5, 0);
    outBuffer_.commit(1 + rtmp::HANDSHAKE_SIZE);
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(outBuffer_.readBuffer(), 1 + rtmp::HANDSHAKE_SIZE),
    [this, self](const boost::system::error_code & ec, std::size_t) {
        if(!ec) {
            outBuffer_.clear();
            doReadS0S1();
        } else if(ec != boost::asio::error::operation_aborted) {
            stopSession();
        }
    });
}

void RtmpSession::doReadS0S1()
{
    auto self(shared_from_this());
    inBuffer_.reserve(1 + rtmp::HANDSHAKE_SIZE);
    boost::asio::async_read(socket_, boost::asio::buffer(inBuffer_.writeBuffer(), 1 + rtmp::HANDSHAKE_SIZE),
    [this, self](const boost::system::error_code & ec, std::size_t len) {
        if(!ec) {
            if(!parseS0S1GenerateC2()) {
                stopSession();
            }
        }  else if(ec != boost::asio::error::operation_aborted) {
            stopSession();
        }
    });
}

bool RtmpSession::parseS0S1GenerateC2()
{
    if(*inBuffer_.readBuffer() != rtmp::HANDSHAKE_VERSION) {
        return false;
    }
    // echo s1
    outBuffer_.append(inBuffer_.readBuffer() + 1, rtmp::HANDSHAKE_SIZE);
    inBuffer_.clear();
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(outBuffer_.readBuffer(), rtmp::HANDSHAKE_SIZE),
    [this, self](const boost::system::error_code & ec, std::size_t) {
        if(!ec) {
            outBuffer_.clear();
            doReadC2S2();
        } else if(ec != boost::asio::error::operation_aborted) {
            stopSession();
        }
    });
    return true;
}

void RtmpSession::doReadC2S2()
{
    auto self(shared_from_this());
    boost::asio::async_read(socket_, boost::asio::buffer(inBuffer_.writeBuffer(), rtmp::HANDSHAKE_SIZE),
    [this, self](const boost::system::error_code & ec, std::size_t len) {
        if(!ec) {
            inBuffer_.commit(rtmp::HANDSHAKE_SIZE);
            //TODO:XXX
            inBuffer_.clear();
            SPDLOG_INFO("RTMP session {}, handshake done", (void *)this);
            doReadChunk();
        }  else if(ec != boost::asio::error::operation_aborted) {
            SPDLOG_ERROR("RTMP session {}, fail to read handshake c2/s2", (void *)this);
            stopSession();
        }
    });
}

void RtmpSession::stopSession()
{
    SPDLOG_INFO("RTMP session {}, stop and free this session", (void *)this);
    if(dir_ != Direction::NONE) {
        assert(stream_);
        stream_->stop(shared_from_this());
    } else {
        server_.stop(shared_from_this());
    }
}

void RtmpSession::doReadChunk()
{
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(inBuffer_.writeBuffer(), inBuffer_.writableSize()),
    [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
        if(!ec) {
            inBuffer_.commit(bytes_transferred);
            while(inBuffer_.readableSize() > 0) {
                if(readingChunkHeader_) {
                    if(!decodeChunkHeader()) {
                        break;
                    }
                } else {
                    if(!decodeChunkPayload()) {
                        break;
                    }
                }
            }
            doReadChunk();
        } else if(ec != boost::asio::error::operation_aborted) {
            stopSession();
        }
    });
}

bool RtmpSession::decodeChunkHeader()
{
    const uint8_t *data = inBuffer_.readBuffer();
    uint32_t offset = 0, readableSize = inBuffer_.readableSize();
    // chunk flags
    if(readableSize < 1) {
        return false;
    }
    uint8_t flags = *(data + offset);
    chunkHeaderFmt_ = flags >> 6;
    chunkHeaderCid_ = flags & 0x3f;
    offset += 1;
    readableSize -= 1;
    // possible cid >= 64
    if(chunkHeaderCid_ == 0) {
        if(readableSize < 1) {
            return false;
        }
        chunkHeaderCid_ = 64 + (uint16_t) * (data + offset);
        offset += 1;
        readableSize -= 1;
    } else if(chunkHeaderCid_ == 1) {
        if(readableSize < 2) {
            return false;
        }
        uint16_t cid;
        loadLE<uint16_t, 16>(data + offset, cid);
        chunkHeaderCid_ = 64 + cid;
        offset += 2;
        readableSize -= 2;
    }
    // chunk header: 11/7/3/0 bytes
    if(readableSize < rtmp::ChunkHeaderSize[chunkHeaderFmt_]) {
        return false;
    }
    // new or previous message slot
    auto m = getMessage(chunkHeaderCid_);
    if(!m) {
        SPDLOG_ERROR("RTMP session {}, cannot get message slot for cid={}", (void *)this, chunkHeaderCid_);
        stopSession();
        return false;
    }
    if(chunkHeaderFmt_ <= rtmp::CHUNK_TYPE_2) {
        loadBE<uint32_t, 24>(data + offset, m->h.timestamp);
        offset += 3;
        readableSize -= 3;
    }
    if(chunkHeaderFmt_ <= rtmp::CHUNK_TYPE_1) {
        loadBE<uint32_t, 24>(data + offset, m->h.length);
        offset += 3;
        readableSize -= 3;
        loadBE<uint8_t, 8>(data + offset, m->h.type);
        offset += 1;
        readableSize -= 1;
    }
    if(chunkHeaderFmt_ == rtmp::CHUNK_TYPE_0) {
        loadLE<uint32_t, 32>(data + offset, m->h.sid);
        offset += 4;
        readableSize -= 4;
    }
    if(m->h.type > rtmp::TYPE_METADATA) {
        SPDLOG_ERROR("RTMP session {}, invalid message type {}", (void *)this, m->h.type);
        stopSession();
        return false;
    }
    // possible extended timestamp
    uint32_t extended = m->h.timestamp;
    if(m->h.timestamp == 0xffffff) {
        if(readableSize < 4) {
            return false;
        }
        loadBE<uint32_t, 32>(data + offset, extended);
        offset += 4;
        readableSize -= 4;
    }
    // initialize the message, in case of first chunk
    if(m->payload.readableSize() == 0) {
        if(rtmp::CHUNK_TYPE_0 == chunkHeaderFmt_) {
            m->h.clock = extended;
        } else {
            m->h.clock += extended;
        }
        m->payload.reserve(m->h.length);
    }
    inBuffer_.erase(offset);
    readingChunkHeader_ = false;
    return true;
}

bool RtmpSession::decodeChunkPayload()
{
    const uint8_t *data = inBuffer_.readBuffer();
    uint32_t readableSize = inBuffer_.readableSize();
    auto m = getMessage(chunkHeaderCid_);
    uint32_t size = std::min((m->h.length - m->payload.readableSize()),
                             (inChunkSize_ - (m->payload.readableSize() % inChunkSize_)));
    size = std::min(readableSize, size);
    m->payload.append(data, size);
    inBuffer_.erase(size);
    if(m->payload.readableSize() >= m->h.length) {
        if(m->h.clock > 0xffffffff) {
            // ignore message
            SPDLOG_INFO("RTMP session {}, ignored message with invalid clock, type={}, size={}", (void *)this, m->h.type, m->h.length);
        } else {
            if(!onMessage(m)) {
                stopSession();
                return false;
            }
        }
        m->payload.clear();
        readingChunkHeader_ = true;
    } else if(0 == (m->payload.readableSize() % inChunkSize_)) {
        readingChunkHeader_ = true;
    } else {
        // incomplete chunk body, keep state
    }
    return true;
}

RtmpMessage *RtmpSession::getMessage(uint32_t cid)
{
    uint32_t pos = cid % RTMP_MAX_CHANNELS;
    if(inMessages_[pos].h.cid == 0 || inMessages_[pos].h.cid == cid) {
        inMessages_[pos].h.cid = cid;
        return &inMessages_[pos];
    }
    for(pos = 0; pos < RTMP_MAX_CHANNELS; pos++) {
        if(inMessages_[pos].h.cid == 0) {
            inMessages_[pos].h.cid = cid;
            return &inMessages_[pos];
        }
    }
    return nullptr;
}

bool RtmpSession::onMessage(RtmpMessage *m)
{
    switch(m->h.type) {
    case rtmp::TYPE_SET_CHUNK_SIZE:
        if(m->payload.readableSize() >= 4) {
            loadBE<uint32_t, 32>(m->payload.readBuffer(), inChunkSize_);
            SPDLOG_DEBUG("RTMP session {}, chunk size changed to {}", (void *)this, inChunkSize_);
        }
        break;
    case rtmp::TYPE_ABORT:
        break;
    case rtmp::TYPE_ACKNOWLEDGEMENT:
        if(m->payload.readableSize() >= 4) {
            uint32_t bytes;
            loadBE<uint32_t, 32>(m->payload.readBuffer(), bytes);
            SPDLOG_DEBUG("RTMP session {}, bytes read: {}", (void *)this, bytes);
        }
        break;
    case rtmp::TYPE_EVENT:
        if(m->payload.readableSize() >= 6) {
            uint16_t arg;
            uint32_t param;
            loadBE<uint16_t, 16>(m->payload.readBuffer(), arg);
            loadBE<uint32_t, 32>(m->payload.readBuffer() + 2, param);
            SPDLOG_DEBUG("RTMP session {}, event {}, param {}", (void *)this, arg, param);
            if(arg == rtmp::EVENT_PING_REQUEST) {
                // echo as EVENT_PING_RESPONSE
                rtmp::MessageEncoder enc(outBuffer_);
                enc.encodePingResponse(timeNow());
                doWrite();
            }
        }
        break;
    case rtmp::TYPE_WINDOW_ACKNOWLEDGEMENT_SIZE:
        if(m->payload.readableSize() >= 4) {
            uint32_t bw;
            loadBE<uint32_t, 32>(m->payload.readBuffer(), bw);
            SPDLOG_DEBUG("RTMP session {}, winack {}", (void *)this, bw);
        }
        break;
    case rtmp::TYPE_SET_PEER_BANDWIDTH:
        if(m->payload.readableSize() >= 5) {
            uint32_t bw;
            loadBE<uint32_t, 32>(m->payload.readBuffer(), bw);
            SPDLOG_DEBUG("RTMP session {}, peer bw {}", (void *)this, bw, (int)(*m->payload.readBuffer()) + 4);
        }
        break;
    case rtmp::TYPE_FLEX_MESSAGE:
    case rtmp::TYPE_INVOKE:
        return onInvoke(m);
        break;
    case rtmp::TYPE_FLEX_STREAM:
    case rtmp::TYPE_DATA:
        return onNotify(m);
        break;
    case rtmp::TYPE_AUDIO:
        stream_->onAudio(m);
        break;
    case rtmp::TYPE_VIDEO:
        stream_->onVideo(m);
        break;
    default:
        break;
    }
    return true;
}

bool RtmpSession::onInvoke(RtmpMessage *m)
{
    std::string_view data = m->payload.stringView();
    if(m->h.type == rtmp::TYPE_FLEX_MESSAGE) {
        data.remove_prefix(1);
    }
    rtmp::AmfDecoder decoder(data);
    rtmp::AmfItem command, trans_id;
    if(!decoder.get(command)) {
        return false;
    }
    if(!decoder.get(trans_id)) {
        return false;
    }
    SPDLOG_DEBUG("RTMP session {}, invoke cmd={}, trans_id={}", (void *)this, command.toString(), trans_id.toString());
    if(command.s == std::string_view("connect", 7)) {
        rtmp::AmfValue args[3] = { std::string_view("app"), std::string_view("tcUrl"), std::string_view("objectEncoding") };
        decoder.get(args, 3);
        SPDLOG_DEBUG("RTMP session {}, connect {}, {}", (void *)this, args[0].toString(), args[1].toString());
        if(args[2].val.type != rtmp::AMF0_UNKNOWN) {
            SPDLOG_ERROR("RTMP session {}, not support AMF version {}", (void *)this, args[2].toString());
            return false;
        }
        app_ = args[0].val.s;
        rtmp::MessageEncoder enc(outBuffer_);
        enc.encodeWindowAck(5000000);
        enc.encodePeerBandwidth(5000000, rtmp::PEER_BANDWITH_LIMIT_TYPE_DYNAMIC);
        enc.encodeStreamBegin();
        enc.encodeSetChunkSize(FLAGS_rtmp_chunk_size);
        outChunkSize_ = FLAGS_rtmp_chunk_size;
        enc.encodeConnectResult(trans_id.n);
        doWrite();
    } else if(command.s == std::string_view("createStream", 12)) {
        rtmp::MessageEncoder enc(outBuffer_);
        enc.encodeCreateStreamResult(trans_id.n);
        doWrite();
    } else if(command.s == std::string_view("publish", 7)) {
        rtmp::AmfItem null_obj, name, pub_type;
        if(!decoder.get(null_obj)) {
            return false;
        }
        if(!decoder.get(name)) {
            return false;
        }
        if(!decoder.get(pub_type)) {
            return false;
        }
        SPDLOG_DEBUG("RTMP session {}, publish {}, {}", (void *)this, name.toString(), pub_type.toString());
        rtmp::MessageEncoder enc(outBuffer_);
        enc.encodeOnStatusPublish(1);
        doWrite();
        dir_ = Direction::INPUT;
        if(!server_.publish(shared_from_this())) {
            stopSession();
        }
    } else if(command.s == std::string_view("play", 4)) {
        rtmp::AmfItem null_obj, name;
        if(!decoder.get(null_obj)) {
            return false;
        }
        if(!decoder.get(name)) {
            return false;
        }
        name_ = name.s;
        SPDLOG_DEBUG("RTMP session {}, play {}", (void *)this, name.toString());
        rtmp::MessageEncoder enc(outBuffer_);
        enc.encodeOnStatusPlay(1);
        doWrite();
        dir_ = Direction::OUTPUT;
        server_.subscribe(shared_from_this());
    } else if(command.s == std::string_view("deleteStream", 12)) {
        rtmp::AmfItem null_obj, sid;
        if(!decoder.get(null_obj)) {
            return false;
        }
        if(!decoder.get(sid)) {
            return false;
        }
        SPDLOG_DEBUG("RTMP session {}, deleteStream {}", (void *)this, sid.n);
    } else if(command.s == std::string_view("onBWDone")) {
        SPDLOG_INFO("RTMP session {}, ignore onBWDone", (void *)this);
    } else if(command.s == std::string_view("_checkbw", 8)) {
        rtmp::MessageEncoder enc(outBuffer_);
        enc.encodeCheckBWResult(trans_id.n);
        doWrite();
    } else if(command.s == std::string_view("_result")) {
    } else if(command.s == std::string_view("onStatus")) {
    } else {
        SPDLOG_ERROR("RTMP session {}, invalid command message {}", (void *)this, command.toString());
    }
    return true;
}

bool RtmpSession::onNotify(RtmpMessage *m)
{
    if(dir_ != Direction::INPUT) {
        return false;
    }
    std::string_view data = m->payload.stringView();
    if(m->h.type == rtmp::TYPE_FLEX_STREAM) {
        data.remove_prefix(1);
    }
    rtmp::AmfDecoder decoder(data);
    rtmp::AmfItem command;
    if(!decoder.get(command)) {
        return false;
    }
    if(command.s == std::string_view("@setDataFrame", 13)) {
        rtmp::AmfItem arg1;
        if(!decoder.get(arg1)) {
            return false;
        }
        if(arg1.s == std::string_view("onMetaData", 10)) {
            return stream_->onMeta(data);
        } else {
            return false;
        }
    } else if(command.s == std::string_view("onMetaData", 10)) {
        return stream_->onMeta(data);
    } else if(command.s == std::string_view("onTextData", 10)) {
        return stream_->onText(m->h.clock, data);
    } else {
        return false;
    }
    return true;
}

void RtmpSession::sendAudioHeader(Buffer *audio)
{
    rtmp::MessageEncoder enc(outBuffer_);
    enc.encodeMessage(*audio, rtmp::TYPE_AUDIO, rtmp::CID_AUDIO, rtmp::MSID_DEFAULT, 0);
    doWrite();
}

void RtmpSession::sendAudio(uint32_t timestamp, std::string_view audio)
{
    rtmp::MessageEncoder enc(outBuffer_);
    enc.encodeMessage(audio, rtmp::TYPE_AUDIO, rtmp::CID_AUDIO, rtmp::MSID_DEFAULT, timestamp);
    doWrite();
}

void RtmpSession::sendVideoHeader(Buffer *video)
{
    rtmp::MessageEncoder enc(outBuffer_);
    enc.encodeMessage(*video, rtmp::TYPE_VIDEO, rtmp::CID_VIDEO, rtmp::MSID_DEFAULT, 0);
}

void RtmpSession::sendVideo(uint32_t timestamp, std::string_view video)
{
    rtmp::MessageEncoder enc(outBuffer_);
    enc.encodeMessage(video, rtmp::TYPE_VIDEO, rtmp::CID_VIDEO, rtmp::MSID_DEFAULT, timestamp);
}

void RtmpSession::sendMetaData(std::string_view metaData)
{
    rtmp::MessageEncoder enc(outBuffer_);
    enc.encodeMeta(metaData);
    doWrite();
}

void RtmpSession::doWrite()
{
    if(!writing_) {
        if(outBuffer_.readableSize() > 0) {
            outBuffer_.swap(outBufferFlush_);
            outBuffer_.clear();
            writing_ = true;
            auto self(shared_from_this());
            boost::asio::async_write(socket_,
                                     boost::asio::buffer(outBufferFlush_.readBuffer(), outBufferFlush_.readableSize()),
            [this, self](const boost::system::error_code & ec, std::size_t) {
                if(!ec) {
                    outBufferFlush_.clear();
                    writing_ = false;
                    doWrite();
                } else if(ec != boost::asio::error::operation_aborted) {
                    SPDLOG_ERROR("RTMP session {}, fail to write", (void *)this);
                    stopSession();
                }
            });
        }
    }
}
}
