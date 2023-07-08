#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "Buffer.hpp"

namespace ms777::rtmp {
constexpr uint8_t HANDSHAKE_VERSION = 3;
constexpr std::size_t HANDSHAKE_SIZE = 1536;

constexpr uint8_t TYPE_SET_CHUNK_SIZE = 1;
constexpr uint8_t TYPE_ABORT = 2;
constexpr uint8_t TYPE_ACKNOWLEDGEMENT = 3;
constexpr uint8_t TYPE_EVENT = 4;
constexpr uint8_t TYPE_WINDOW_ACKNOWLEDGEMENT_SIZE = 5;
constexpr uint8_t TYPE_SET_PEER_BANDWIDTH = 6;
constexpr uint8_t TYPE_AUDIO = 8;
constexpr uint8_t TYPE_VIDEO = 9;
constexpr uint8_t TYPE_FLEX_STREAM = 15;
constexpr uint8_t TYPE_FLEX_OBJECT = 16;
constexpr uint8_t TYPE_FLEX_MESSAGE = 17;
constexpr uint8_t TYPE_DATA = 18;
constexpr uint8_t TYPE_SHARED_OBJECT = 19;
constexpr uint8_t TYPE_INVOKE = 20;
constexpr uint8_t TYPE_METADATA = 22;

constexpr uint16_t EVENT_STREAM_BEGIN  = 0;
constexpr uint16_t EVENT_STREAM_EOF = 1;
constexpr uint16_t EVENT_BUFFER_TIME = 3;
constexpr uint16_t EVENT_PING_REQUEST = 6;
constexpr uint16_t EVENT_PING_RESPONSE = 7;

constexpr uint8_t PEER_BANDWITH_LIMIT_TYPE_HARD = 0;
constexpr uint8_t PEER_BANDWITH_LIMIT_TYPE_SOFT = 1;
constexpr uint8_t PEER_BANDWITH_LIMIT_TYPE_DYNAMIC = 2;

constexpr uint32_t MSID_PROTOCOL_CONTROL_MESSAGE = 0;
constexpr uint32_t MSID_DEFAULT = 1;

constexpr uint32_t DEFAULT_CHUNK_SIZE = 128;

constexpr uint32_t CID_PROTOCOL_CONTROL = 2;
constexpr uint32_t CID_OVER_CONNECTION = 3;
constexpr uint32_t CID_OVER_STREAM = 5;
constexpr uint32_t CID_META = 5;
constexpr uint32_t CID_AUDIO = 6;
constexpr uint32_t CID_VIDEO = 7;

constexpr uint8_t CHUNK_TYPE_0 = 0; // 11-bytes: timestamp(3) + length(3) + stream type(1) + stream id(4)
constexpr uint8_t CHUNK_TYPE_1 = 1; // 7-bytes: delta(3) + length(3) + stream type(1)
constexpr uint8_t CHUNK_TYPE_2 = 2; // 3-bytes: delta(3)
constexpr uint8_t CHUNK_TYPE_3 = 3; // 0-byte

constexpr uint32_t ChunkHeaderSize[] = { 11, 7, 3, 0 };

constexpr uint32_t TRANSACTION_ID_CLIENT_CONNECT = 1;
constexpr uint32_t TRANSACTION_ID_CLIENT_CREATE_STREAM = 2;
constexpr uint32_t TRANSACTION_ID_CLIENT_PLAY = 3;
constexpr uint32_t TRANSACTION_ID_CLIENT_PUBLISH = 3;

constexpr uint8_t AMF0_UNKNOWN = 0xFF;
constexpr uint8_t AMF0_NUMBER = 0x00;
constexpr uint8_t AMF0_BOOLEAN = 0x01;
constexpr uint8_t AMF0_STRING = 0x02;
constexpr uint8_t AMF0_OBJECT = 0x03;
constexpr uint8_t AMF0_MOVIECLIP = 0x04;
constexpr uint8_t AMF0_NULL = 0x05;
constexpr uint8_t AMF0_UNDEFINED = 0x06;
constexpr uint8_t AMF0_REFERENCE = 0x07;
constexpr uint8_t AMF0_ECMA_ARRAY = 0x08;
constexpr uint8_t AMF0_OBJECT_END = 0x09;
constexpr uint8_t AMF0_STRICT_ARRAY = 0x0A;
constexpr uint8_t AMF0_DATE = 0x0B;
constexpr uint8_t AMF0_LONG_STRING = 0x0C;
constexpr uint8_t AMF0_UNSUPPORTED = 0x0D;
constexpr uint8_t AMF0_RECORDSET = 0x0E;
constexpr uint8_t AMF0_XML_DOCUMENT = 0x0F;
constexpr uint8_t AMF0_TYPED_OBJECT = 0x10;

struct AmfDate {
    double ms;
    uint32_t tz;
};

struct AmfItem {
    uint8_t type;
    union {
        double n;
        std::string_view s;
        bool b;
        AmfDate d;
    };

    AmfItem();
    AmfItem(uint8_t type_);
    AmfItem(double n_);
    AmfItem(bool b_);
    AmfItem(std::string_view s_);
    AmfItem(AmfDate d_);
    AmfItem(const AmfItem &o);

    std::string toString();
};

struct AmfValue {
    std::string_view key;
    AmfItem val;

    AmfValue();
    AmfValue(std::string_view k);
    AmfValue(const AmfValue &o);

    std::string toString();
};

class AmfDecoder
{
public:
    AmfDecoder(std::string_view &data);

    bool get(AmfItem &val);
    bool get(AmfValue *items, std::size_t count);
    bool get(std::vector<AmfValue> &items);

private:
    void copy(AmfValue *item, AmfValue *items, std::size_t count);

    bool getNumber(double &val);
    bool getBool(bool &val);
    bool getString(std::string_view &val);
    bool getLongString(std::string_view &val);
    bool getDate(AmfDate &val);

    template<class T, std::size_t n_bits>
    bool getBE(T &output)
    {
        if(data_.size() >= (n_bits / 8)) {
            loadBE<T, n_bits>((const void *)data_.data(), output);
            data_.remove_prefix(n_bits / 8);
            return true;
        } else {
            return false;
        }
    }

    template<class T, std::size_t n_bits>
    bool getLE(T &output)
    {
        if(data_.size() >= (n_bits / 8)) {
            loadLE<T, n_bits>((const void *)data_.data(), output);
            data_.remove_prefix(n_bits / 8);
            return true;
        } else {
            return false;
        }
    }

    bool get(std::string_view &v, uint32_t length);

private:
    std::string_view data_;
};

class AmfEncoder
{
public:
    AmfEncoder(Buffer &data);

    void putNumber(double val);
    void putString(std::string_view val);
    void putStringNoType(std::string_view val);
    void putLongString(std::string_view val);
    void putBool(bool val);
    void putNull();
    void putObjectBegin();
    void putObjectValue(std::string_view k, std::string_view v);
    void putObjectValue(std::string_view k, double v);
    void putObjectEnd();

private:
    void put(std::string_view v);

private:
    Buffer &data_;
};

class MessageEncoder
{
public:
    MessageEncoder(Buffer &output);

    void encodeWindowAck(uint32_t size);
    void encodeAck(uint32_t size);
    void encodePeerBandwidth(uint32_t size, uint8_t type);
    void encodeSetChunkSize(uint32_t size);
    void encodePingResponse(uint32_t timestamp);
    void encodeStreamBegin();
    void encodeStreamEof();
    void encodeConnectResult(double tid);
    void encodeOnStatusPublish(uint32_t sid);
    void encodeOnStatusPlay(uint32_t sid);
    void encodeCreateStreamResult(double tid);
    void encodeConnect(const char *app, const char *swf_url, const char *tc_url, uint32_t tid);
    void encodeReleaseStream(const char *stream_name, uint32_t tid);
    void encodeFcPublish(const char *stream_name, uint32_t tid);
    void encodeCreateStream(uint32_t tid);
    void encodePublish(const char *app, const char *stream_name, int stream_id, uint32_t tid);
    void encodePlay(const char *stream_name, int stream_id, uint32_t tid);
    void encodeCheckBWResult(double tid);
    void encodeMessage(Buffer &payload, uint8_t type, uint8_t cid, uint32_t sid, uint32_t timestamp);
    void encodeMessage(std::string_view payload, uint8_t type, uint8_t cid, uint32_t sid, uint32_t timestamp);
    void encodeMeta(std::string_view metaData);

private:
    void encodeChunkHeader0(uint8_t type, uint32_t length, uint8_t cid, uint32_t sid, uint32_t timestamp);
    void encodeChunkHeader3(uint8_t cid);

private:
    Buffer &output_;
};
}
