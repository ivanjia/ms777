#pragma once
#include <cstdint>
#include <string>
#include "Endian.hpp"

namespace ms777 {
class Buffer
{
public:
    Buffer();
    Buffer(uint32_t capacity);
    ~Buffer();

    void clear();
    void swap(Buffer &other);

    const uint8_t *readBuffer();
    uint32_t readableSize();
    std::string_view stringView();
    void erase(uint32_t n);

    uint8_t *writeBuffer();
    uint32_t writableSize();
    void commit(uint32_t n);

    void reserve(uint32_t n);
    void append(const uint8_t *data, uint32_t n);
    void append(std::string_view data);

    template<class T, std::size_t n_bits>
    void putBE(T input)
    {
        reserve(n_bits / 8);
        storeBE<T, n_bits>(writeBuffer(), input);
        commit(n_bits / 8);
    }

    template<class T, std::size_t n_bits>
    void putLE(T input)
    {
        reserve(n_bits / 8);
        storeLE<T, n_bits>(writeBuffer(), input);
        commit(n_bits / 8);
    }

    void normalize();

private:
    uint8_t *start_;
    uint32_t capacity_;
    uint32_t readPos_;
    uint32_t writePos_;
};
}
