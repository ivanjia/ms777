#pragma once
#include <boost/endian/buffers.hpp>

namespace ms777 {
template<class T, std::size_t n_bits>
inline void loadBE(const void *input, T &output)
{
    output = ((boost::endian::endian_buffer<boost::endian::order::big, T, n_bits> *)input)->value();
}

template<class T, std::size_t n_bits>
inline void loadLE(const void *input, T &output)
{
    output = ((boost::endian::endian_buffer<boost::endian::order::little, T, n_bits> *)input)->value();
}

template<class T, std::size_t n_bits>
inline void storeBE(void *output, T input)
{
    *((boost::endian::endian_buffer<boost::endian::order::big, T, n_bits> *)output) = input;
}

template<class T, std::size_t n_bits>
inline void storeLE(void *output, T input)
{
    *((boost::endian::endian_buffer<boost::endian::order::little, T, n_bits> *)output) = input;
}
}
