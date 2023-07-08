#include <cassert>
#include "Rtmp.hpp"
#include "Conf.hpp"

namespace ms777::rtmp {
static thread_local Buffer messageBody_(1024);

AmfItem::AmfItem() : type(AMF0_UNKNOWN)
{
}

AmfItem::AmfItem(uint8_t type_) : type(type_)
{
}

AmfItem::AmfItem(double n_) : type(AMF0_NUMBER), n(n_)
{
}

AmfItem::AmfItem(bool b_) : type(AMF0_BOOLEAN), b(b_)
{
}

AmfItem::AmfItem(std::string_view s_) : s(s_)
{
    if(s_.size() > 65535) {
        type = AMF0_LONG_STRING;
    } else {
        type = AMF0_STRING;
    }
}

AmfItem::AmfItem(AmfDate d_) : type(AMF0_DATE), d(d_)
{
}

AmfItem::AmfItem(const AmfItem &o)
{
    type = o.type;
    switch(type) {
    case AMF0_NUMBER:
        n = o.n;
        break;
    case AMF0_STRING:
    case AMF0_LONG_STRING:
        s = o.s;
        break;
    case AMF0_DATE:
        d = o.d;
        break;
    case AMF0_BOOLEAN:
        b = o.b;
        break;
    default:
        break;
    }
}

std::string AmfItem::toString()
{
    switch(type) {
    case AMF0_NUMBER:
        return std::to_string(n);
    case AMF0_STRING:
    case AMF0_LONG_STRING:
        return std::string(s.data(), s.size());
    case AMF0_DATE:
        return std::to_string(d.ms) + ",tz=" + std::to_string(d.tz);
    case AMF0_BOOLEAN:
        return std::to_string(b);
    default:
        return std::to_string(type);
    }
}

AmfValue::AmfValue()
{
}

AmfValue::AmfValue(std::string_view k) : key(k)
{
}

AmfValue::AmfValue(const AmfValue &o) : key(o.key), val(o.val)
{
}

std::string AmfValue::toString()
{
    return std::string(key.data(), key.size()) + "=" + val.toString();
}

AmfDecoder::AmfDecoder(std::string_view &data) : data_(data)
{
}

bool AmfDecoder::get(AmfItem &val)
{
    if(!getBE<uint8_t, 8>(val.type)) {
        return false;
    }
    switch(val.type) {
    case AMF0_NUMBER:
        return getNumber(val.n);
    case AMF0_BOOLEAN:
        return getBool(val.b);
    case AMF0_STRING:
        return getString(val.s);
    case AMF0_LONG_STRING:
        return getLongString(val.s);
    case AMF0_DATE:
        return getDate(val.d);
    case AMF0_NULL:
        return true;
    case AMF0_OBJECT_END:
        return true;
    default:
        return false;
    }
}

bool AmfDecoder::get(AmfValue *items, std::size_t count)
{
    uint8_t type = AMF0_UNKNOWN;
    if(!getBE<uint8_t, 8>(type)) {
        return false;
    }
    if(type != AMF0_OBJECT) {
        return false;
    }
    while(true) {
        AmfValue i;
        if(!getString(i.key)) {
            return false;
        }
        if(!get(i.val)) {
            return false;
        }
        if(i.val.type != AMF0_OBJECT_END) {
            copy(&i, items, count);
            continue;
        } else {
            break;
        }
    }
    return true;
}

bool AmfDecoder::get(std::vector<AmfValue> &items)
{
    uint8_t type = AMF0_UNKNOWN;
    if(!getBE<uint8_t, 8>(type)) {
        return false;
    }
    if(type != AMF0_OBJECT) {
        return false;
    }
    while(true) {
        AmfValue i;
        if(!getString(i.key)) {
            return false;
        }
        if(!get(i.val)) {
            return false;
        }
        if(i.val.type != AMF0_OBJECT_END) {
            items.push_back(i);
            continue;
        } else {
            break;
        }
    }
    return true;
}

void AmfDecoder::copy(AmfValue *item, AmfValue *items, std::size_t count)
{
    for(std::size_t i = 0; i < count; i++) {
        if(items[i].key == item->key) {
            items[i].val = item->val;
        }
    }
}

bool AmfDecoder::getNumber(double &val)
{
    return getBE<double, 64>(val);
}

bool AmfDecoder::getBool(bool &val)
{
    uint8_t tmp = 0;
    if(!getBE<uint8_t, 8>(tmp)) {
        return false;
    }
    val = (tmp != 0);
    return true;
}

bool AmfDecoder::getString(std::string_view &val)
{
    uint16_t len = 0;
    if(!getBE<uint16_t, 16>(len)) {
        return false;
    }
    return get(val, len);
}

bool AmfDecoder::getLongString(std::string_view &val)
{
    uint32_t len = 0;
    if(!getBE<uint32_t, 32>(len)) {
        return false;
    }
    return get(val, len);
}

bool AmfDecoder::getDate(AmfDate &val)
{
    if(!getNumber(val.ms)) {
        return false;
    }
    return getBE<uint32_t, 32>(val.tz);
}

bool AmfDecoder::get(std::string_view &v, uint32_t length)
{
    if(data_.size() >= length) {
        v = std::string_view(data_.data(), (std::size_t)length);
        data_.remove_prefix(length);
        return true;
    } else {
        return false;
    }
}

AmfEncoder::AmfEncoder(Buffer &data) : data_(data)
{
}

void AmfEncoder::putNumber(double val)
{
    data_.putBE<uint8_t, 8>(AMF0_NUMBER);
    data_.putBE<double, 64>(val);
}

void AmfEncoder::putString(std::string_view val)
{
    if(val.size() > 65535) {
        return putLongString(val);
    } else {
        data_.putBE<uint8_t, 8>(AMF0_STRING);
        data_.putBE<uint16_t, 16>(val.size());
        put(val);
    }
}

void AmfEncoder::putStringNoType(std::string_view val)
{
    data_.putBE<uint16_t, 16>(val.size());
    put(val);
}

void AmfEncoder::putLongString(std::string_view val)
{
    data_.putBE<uint8_t, 8>(AMF0_LONG_STRING);
    data_.putBE<uint32_t, 32>(val.size());
    put(val);
}

void AmfEncoder::putBool(bool val)
{
    data_.putBE<uint8_t, 8>(AMF0_BOOLEAN);
    data_.putBE<uint8_t, 8>((val ? 1 : 0));
}

void AmfEncoder::putNull()
{
    data_.putBE<uint8_t, 8>(AMF0_NULL);
}

void AmfEncoder::putObjectBegin()
{
    data_.putBE<uint8_t, 8>(AMF0_OBJECT);
}

void AmfEncoder::putObjectValue(std::string_view k, std::string_view v)
{
    putStringNoType(k);
    putString(v);
}

void AmfEncoder::putObjectValue(std::string_view k, double v)
{
    putStringNoType(k);
    putNumber(v);
}

void AmfEncoder::putObjectEnd()
{
    data_.putBE<uint16_t, 16>(0); // empty string
    data_.putBE<uint8_t, 8>(AMF0_OBJECT_END);
}

void AmfEncoder::put(std::string_view v)
{
    data_.reserve(v.size());
    memcpy(data_.writeBuffer(), v.data(), v.size());
    data_.commit(v.size());
}

MessageEncoder::MessageEncoder(Buffer &output)
    : output_(output)
{
}

void MessageEncoder::encodeChunkHeader0(uint8_t type, uint32_t length, uint8_t cid, uint32_t sid, uint32_t timestamp)
{
    assert(cid < 64);
    output_.putBE<uint8_t, 8>(cid); // (CHUNK_TYPE_0(0) << 6) | cid
    output_.putBE<uint32_t, 24>(timestamp); // timestamp
    output_.putBE<uint32_t, 24>(length); // length
    output_.putBE<uint8_t, 8>(type); // type
    output_.putLE<uint32_t, 32>(sid); // sid = 0
}

void MessageEncoder::encodeChunkHeader3(uint8_t cid)
{
    assert(cid < 64);
    output_.putBE<uint8_t, 8>((3 << 6) | cid);
}

void MessageEncoder::encodeWindowAck(uint32_t size)
{
    encodeChunkHeader0(TYPE_WINDOW_ACKNOWLEDGEMENT_SIZE, 4, CID_PROTOCOL_CONTROL, 0, 0);
    output_.putBE<uint32_t, 32>(size); // body
}

void MessageEncoder::encodeAck(uint32_t size)
{
    encodeChunkHeader0(TYPE_ACKNOWLEDGEMENT, 4, CID_PROTOCOL_CONTROL, 0, 0);
    output_.putBE<uint32_t, 32>(size); // body
}

void MessageEncoder::encodePeerBandwidth(uint32_t size, uint8_t type)
{
    encodeChunkHeader0(TYPE_SET_PEER_BANDWIDTH, 5, CID_PROTOCOL_CONTROL, 0, 0);
    output_.putBE<uint32_t, 32>(size); // body
    output_.putBE<uint8_t, 8>(type); // body
}

void MessageEncoder::encodeSetChunkSize(uint32_t size)
{
    encodeChunkHeader0(TYPE_SET_CHUNK_SIZE, 4, CID_PROTOCOL_CONTROL, 0, 0);
    output_.putBE<uint32_t, 32>(size); // body
}

void MessageEncoder::encodePingResponse(uint32_t timestamp)
{
    encodeChunkHeader0(TYPE_EVENT, 6, CID_PROTOCOL_CONTROL, 0, 0);
    output_.putBE<uint16_t, 16>(EVENT_PING_RESPONSE);
    output_.putBE<uint32_t, 32>(timestamp);
}

void MessageEncoder::encodeStreamBegin()
{
    encodeChunkHeader0(TYPE_EVENT, 2, CID_PROTOCOL_CONTROL, 0, 0);
    output_.putBE<uint16_t, 16>(EVENT_STREAM_BEGIN);
}

void MessageEncoder::encodeStreamEof()
{
    encodeChunkHeader0(TYPE_EVENT, 2, CID_PROTOCOL_CONTROL, 0, 0);
    output_.putBE<uint16_t, 16>(EVENT_STREAM_EOF);
}

void MessageEncoder::encodeConnectResult(double tid)
{
    messageBody_.clear();
    AmfEncoder enc(messageBody_);
    enc.putString(std::string_view("_result", 7));
    enc.putNumber(tid);
    enc.putObjectBegin();
    enc.putObjectValue(std::string_view("fmsVer", 6), std::string_view("FMS/3,0,1,123", 13));
    enc.putObjectValue(std::string_view("capabilities", 12), 31.0);
    enc.putObjectEnd();
    enc.putObjectBegin();
    enc.putObjectValue(std::string_view("level", 5), std::string_view("status", 6));
    enc.putObjectValue(std::string_view("code", 4), std::string_view("NetConnection.Connect.Success", 29));
    enc.putObjectValue(std::string_view("description", 11), std::string_view("Connection succeeded.", 21));
    enc.putObjectValue(std::string_view("objectEncoding", 14), 0.0);
    enc.putObjectEnd();
    encodeMessage(messageBody_, TYPE_INVOKE, CID_OVER_CONNECTION, 0, 0);
}

void MessageEncoder::encodeOnStatusPublish(uint32_t sid)
{
    messageBody_.clear();
    AmfEncoder enc(messageBody_);
    enc.putString(std::string_view("onStatus", 8));
    enc.putNumber(0);
    enc.putNull();
    enc.putObjectBegin();
    enc.putObjectValue(std::string_view("level", 5), std::string_view("status", 6));
    enc.putObjectValue(std::string_view("code", 4), std::string_view("NetStream.Publish.Start", 23));
    enc.putObjectValue(std::string_view("description", 11), std::string_view("Start publishing", 16));
    enc.putObjectEnd();
    encodeMessage(messageBody_, TYPE_INVOKE, CID_OVER_STREAM, sid, 0);
}

void MessageEncoder::encodeOnStatusPlay(uint32_t sid)
{
    messageBody_.clear();
    AmfEncoder enc(messageBody_);
    enc.putString(std::string_view("onStatus", 8));
    enc.putNumber(0);
    enc.putNull();
    enc.putObjectBegin();
    enc.putObjectValue(std::string_view("level", 5), std::string_view("status", 6));
    enc.putObjectValue(std::string_view("code", 4), std::string_view("NetStream.Play.Start", 20));
    enc.putObjectValue(std::string_view("description", 11), std::string_view("Start live", 10));
    enc.putObjectEnd();
    encodeMessage(messageBody_, TYPE_INVOKE, CID_OVER_STREAM, sid, 0);
}

void MessageEncoder::encodeCreateStreamResult(double tid)
{
    messageBody_.clear();
    AmfEncoder enc(messageBody_);
    enc.putString(std::string_view("_result", 7));
    enc.putNumber(tid);
    enc.putNull();
    enc.putNumber(MSID_DEFAULT);
    encodeMessage(messageBody_, TYPE_INVOKE, CID_OVER_CONNECTION, 0, 0);
}

void MessageEncoder::encodeConnect(const char *app, const char *swf_url, const char *tc_url, uint32_t tid)
{
    messageBody_.clear();
    AmfEncoder enc(messageBody_);
    enc.putString(std::string_view("connect", 7));
    enc.putNumber(tid);
    enc.putObjectBegin();
    enc.putObjectValue(std::string_view("app", 3), std::string_view(app, strlen(app)));
    enc.putObjectValue(std::string_view("swfUrl", 6), std::string_view(swf_url, strlen(swf_url)));
    enc.putObjectValue(std::string_view("tcUrl", 5), std::string_view(tc_url, strlen(tc_url)));
    enc.putObjectEnd();
    encodeMessage(messageBody_, TYPE_INVOKE, CID_OVER_CONNECTION, 0, 0);
}

void MessageEncoder::encodeReleaseStream(const char *stream_name, uint32_t tid)
{
    messageBody_.clear();
    AmfEncoder enc(messageBody_);
    enc.putString(std::string_view("releaseStream", 13));
    enc.putNumber(tid);
    enc.putNull();
    enc.putString(std::string_view(stream_name, strlen(stream_name)));
    encodeMessage(messageBody_, TYPE_INVOKE, CID_OVER_CONNECTION, 0, 0);
}

void MessageEncoder::encodeFcPublish(const char *stream_name, uint32_t tid)
{
    messageBody_.clear();
    AmfEncoder enc(messageBody_);
    enc.putString(std::string_view("FCPublish", 9));
    enc.putNumber(tid);
    enc.putNull();
    enc.putString(std::string_view(stream_name, strlen(stream_name)));
    encodeMessage(messageBody_, TYPE_INVOKE, CID_OVER_CONNECTION, 0, 0);
}

void MessageEncoder::encodeCreateStream(uint32_t tid)
{
    messageBody_.clear();
    AmfEncoder enc(messageBody_);
    enc.putString(std::string_view("createStream", 12));
    enc.putNumber(tid);
    enc.putNull();
    encodeMessage(messageBody_, TYPE_INVOKE, CID_OVER_CONNECTION, 0, 0);
}

void MessageEncoder::encodePublish(const char *app, const char *stream_name, int stream_id, uint32_t tid)
{
    messageBody_.clear();
    AmfEncoder enc(messageBody_);
    enc.putString(std::string_view("publish", 7));
    enc.putNumber(tid);
    enc.putNull();
    enc.putString(std::string_view(stream_name, strlen(stream_name)));
    enc.putString(std::string_view(app, strlen(app)));
    encodeMessage(messageBody_, TYPE_INVOKE, CID_OVER_STREAM, stream_id, 0);
}

void MessageEncoder::encodeCheckBWResult(double tid)
{
    messageBody_.clear();
    AmfEncoder enc(messageBody_);
    enc.putString(std::string_view("_result", 7));
    enc.putNumber(tid);
    enc.putNull();
    encodeMessage(messageBody_, TYPE_INVOKE, CID_OVER_STREAM, 0, 0);
}

void MessageEncoder::encodePlay(const char *stream_name, int stream_id, uint32_t tid)
{
    messageBody_.clear();
    AmfEncoder enc(messageBody_);
    enc.putString(std::string_view("play", 4));
    enc.putNumber(tid);
    enc.putNull();
    enc.putString(std::string_view(stream_name, strlen(stream_name)));
    encodeMessage(messageBody_, TYPE_INVOKE, CID_OVER_STREAM, stream_id, 0);
}

void MessageEncoder::encodeMeta(std::string_view metaData)
{
    messageBody_.clear();
    AmfEncoder enc(messageBody_);
    enc.putString(std::string_view("@setDataFrame", 13));
    enc.putString(std::string_view("onMetaData", 10));
    messageBody_.append(metaData);
    encodeMessage(messageBody_, TYPE_DATA, CID_AUDIO, MSID_DEFAULT, 0);
}

void MessageEncoder::encodeMessage(Buffer &payload, uint8_t type, uint8_t cid, uint32_t sid, uint32_t timestamp)
{
    encodeMessage(payload.stringView(), type, cid, sid, timestamp);
}

void MessageEncoder::encodeMessage(std::string_view payload, uint8_t type, uint8_t cid, uint32_t sid, uint32_t timestamp)
{
    encodeChunkHeader0(type, payload.size(), cid, sid, timestamp);
    uint32_t chunk_size = std::min(FLAGS_rtmp_chunk_size, (uint32_t)payload.size());
    output_.append((const uint8_t *)payload.data(), chunk_size);
    uint32_t offset = chunk_size, len = payload.size() - chunk_size;
    while(len > 0) {
        encodeChunkHeader3(cid);
        chunk_size = std::min(len, FLAGS_rtmp_chunk_size);
        output_.append((const uint8_t *)payload.data() + offset, chunk_size);
        len -= chunk_size;
        offset += chunk_size;
    }
}
}
