#include <cassert>
#include <cstring>
#include <utility>
#include "Buffer.hpp"

namespace ms777 {

Buffer::Buffer() : capacity_(0), readPos_(0), writePos_(0)
{
}

Buffer::Buffer(uint32_t capacity) : capacity_(capacity), readPos_(0), writePos_(0)
{
    start_ = new uint8_t[capacity_];
}

Buffer::~Buffer()
{
    delete []start_;
}

void Buffer::clear()
{
    readPos_ = writePos_ = 0;
}

void Buffer::swap(Buffer &other)
{
    std::swap(start_, other.start_);
    std::swap(capacity_, other.capacity_);
    std::swap(readPos_, other.readPos_);
    std::swap(writePos_, other.writePos_);
}

const uint8_t *Buffer::readBuffer()
{
    return start_ + readPos_;
}

uint32_t Buffer::readableSize()
{
    return writePos_ - readPos_;
}

std::string_view Buffer::stringView()
{
    return std::string_view((const char *)readBuffer(), readableSize());
}

void Buffer::erase(uint32_t n)
{
    assert((writePos_ - readPos_) >= n);
    readPos_ += n;
}

uint8_t *Buffer::writeBuffer()
{
    normalize();
    return (start_ + writePos_);
}

uint32_t Buffer::writableSize()
{
    return capacity_ - writePos_;
}

void Buffer::commit(uint32_t n)
{
    assert((capacity_ - writePos_) >= n);
    writePos_ += n;
}

void Buffer::reserve(uint32_t n)
{
    if((capacity_ - (writePos_ - readPos_)) >= n) {
        normalize();
    } else {
        uint32_t newCapacity = (writePos_ - readPos_) + n;
        Buffer newBuffer(newCapacity);
        newBuffer.append(readBuffer(), readableSize());
        swap(newBuffer);
    }
}

void Buffer::append(const uint8_t *data, uint32_t n)
{
    reserve(n);
    memcpy(writeBuffer(), data, n);
    commit(n);
}

void Buffer::append(std::string_view data)
{
    append((const uint8_t *)data.data(), data.size());
}

void Buffer::normalize()
{
    if(readPos_ > 0) {
        if(writePos_ > readPos_) {
            memmove(start_, start_ + readPos_, writePos_ - readPos_);
            writePos_ -= readPos_;
            readPos_ = 0;
        } else {
            readPos_ = writePos_ = 0;
        }
    }
}
}
