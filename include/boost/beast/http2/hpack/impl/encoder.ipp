//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_HPACK_IMPL_ENCODER_IPP
#define BOOST_BEAST_HTTP2_HPACK_IMPL_ENCODER_IPP

#include <boost/beast/http2/hpack/encoder.hpp>
#include <boost/beast/http2/hpack/integer.hpp>
#include <boost/beast/http2/hpack/huffman.hpp>
#include <cstring>

namespace boost {
namespace beast {
namespace http2 {
namespace hpack {

std::size_t
encoder::encode_string(
    std::uint8_t* dest,
    std::size_t dest_size,
    string_view str)
{
    // Try Huffman encoding first, use it if it saves space
    auto const huff_size = huffman_encoded_size(
        reinterpret_cast<std::uint8_t const*>(str.data()),
        str.size());

    if(huff_size < str.size())
    {
        // Use Huffman encoding
        auto n = encode_integer(dest, dest_size,
            huff_size, 7, 0x80);
        if(n == 0)
            return 0;

        auto encoded = huffman_encode(dest + n, dest_size - n,
            reinterpret_cast<std::uint8_t const*>(str.data()),
            str.size());
        if(encoded == 0)
            return 0;

        return n + encoded;
    }
    else
    {
        // Use raw string
        auto n = encode_integer(dest, dest_size,
            str.size(), 7, 0x00);
        if(n == 0)
            return 0;

        if(n + str.size() > dest_size)
            return 0;

        std::memcpy(dest + n, str.data(), str.size());
        return n + str.size();
    }
}

std::size_t
encoder::encode_field(
    std::uint8_t* dest,
    std::size_t dest_size,
    string_view name,
    string_view value,
    bool indexing,
    bool sensitive)
{
    std::size_t pos = 0;

    // Search for the name (and optionally value) in tables
    bool name_match = false;
    std::size_t index = 0;

    // Search static table first
    index = static_table_find(name, value, name_match);
    if(index > 0 && !name_match)
    {
        // Exact match in static table -> indexed representation
        auto n = encode_integer(dest + pos, dest_size - pos,
            index, 7, 0x80);
        if(n == 0)
            return 0;
        return pos + n;
    }

    // Search dynamic table if no exact match yet
    if(index == 0 || name_match)
    {
        bool dyn_name_match = false;
        auto dyn_idx = dyn_table_.find(name, value, dyn_name_match);
        if(dyn_idx != static_cast<std::size_t>(-1))
        {
            std::size_t absolute_idx = static_table_size + 1 + dyn_idx;
            if(!dyn_name_match)
            {
                // Exact match in dynamic table -> indexed
                auto n = encode_integer(dest + pos, dest_size - pos,
                    absolute_idx, 7, 0x80);
                if(n == 0)
                    return 0;
                return pos + n;
            }
            if(!name_match)
            {
                index = absolute_idx;
                name_match = true;
            }
        }
    }

    // Literal representation
    std::uint8_t pattern;
    std::uint8_t prefix_bits;

    if(sensitive)
    {
        // Never Indexed (Section 6.2.3)
        pattern = 0x10;
        prefix_bits = 4;
    }
    else if(indexing)
    {
        // With Incremental Indexing (Section 6.2.1)
        pattern = 0x40;
        prefix_bits = 6;
    }
    else
    {
        // Without Indexing (Section 6.2.2)
        pattern = 0x00;
        prefix_bits = 4;
    }

    if(name_match && index > 0)
    {
        // Indexed name
        auto n = encode_integer(dest + pos, dest_size - pos,
            index, prefix_bits, pattern);
        if(n == 0)
            return 0;
        pos += n;
    }
    else
    {
        // New name
        auto n = encode_integer(dest + pos, dest_size - pos,
            0, prefix_bits, pattern);
        if(n == 0)
            return 0;
        pos += n;

        n = encode_string(dest + pos, dest_size - pos, name);
        if(n == 0)
            return 0;
        pos += n;
    }

    // Encode value
    auto n = encode_string(dest + pos, dest_size - pos, value);
    if(n == 0)
        return 0;
    pos += n;

    // Add to dynamic table if indexing
    if(indexing && !sensitive)
        dyn_table_.insert(name, value);

    return pos;
}

std::size_t
encoder::encode_table_size_update(
    std::uint8_t* dest,
    std::size_t dest_size,
    std::size_t new_size)
{
    dyn_table_.set_max_size(new_size);
    return encode_integer(dest, dest_size, new_size, 5, 0x20);
}

} // hpack
} // http2
} // beast
} // boost

#endif
