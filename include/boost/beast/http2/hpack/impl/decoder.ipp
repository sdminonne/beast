//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_HPACK_IMPL_DECODER_IPP
#define BOOST_BEAST_HTTP2_HPACK_IMPL_DECODER_IPP

#include <boost/beast/http2/hpack/decoder.hpp>
#include <boost/beast/http2/hpack/integer.hpp>
#include <boost/beast/http2/hpack/huffman.hpp>
#include <boost/beast/http2/error.hpp>

namespace boost {
namespace beast {
namespace http2 {
namespace hpack {

table_entry
decoder::lookup(std::size_t index, error_code& ec) const
{
    if(index == 0)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::hpack_bad_index);
        return {"", ""};
    }

    if(index <= static_table_size)
        return static_table_entry(index);

    std::size_t dyn_idx = index - static_table_size - 1;
    if(dyn_idx >= dyn_table_.size())
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::hpack_bad_index);
        return {"", ""};
    }

    return dyn_table_.at(dyn_idx);
}

void
decoder::decode_string(
    std::string& out,
    std::uint8_t const* data,
    std::size_t length,
    std::size_t& consumed,
    error_code& ec)
{
    if(length == 0)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::need_more);
        return;
    }

    bool huffman = (data[0] & 0x80) != 0;

    std::uint64_t str_len;
    std::size_t int_consumed;
    decode_integer(data, length, str_len, 7, int_consumed, ec);
    if(ec)
        return;

    if(int_consumed + str_len > length)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::need_more);
        return;
    }

    if(huffman)
    {
        huffman_decode(out, data + int_consumed,
            static_cast<std::size_t>(str_len), ec);
        if(ec)
            return;
    }
    else
    {
        out.assign(reinterpret_cast<char const*>(
            data + int_consumed), static_cast<std::size_t>(str_len));
    }

    consumed = int_consumed + static_cast<std::size_t>(str_len);
}

void
decoder::decode_indexed(
    std::uint8_t const* data,
    std::size_t length,
    std::size_t& consumed,
    decode_handler& handler,
    error_code& ec)
{
    // RFC 7541 Section 6.1: Indexed Header Field Representation
    // Starts with 1-bit pattern '1'
    std::uint64_t index;
    decode_integer(data, length, index, 7, consumed, ec);
    if(ec)
        return;

    auto entry = lookup(static_cast<std::size_t>(index), ec);
    if(ec)
        return;

    handler.on_header(entry.name, entry.value, false, ec);
}

void
decoder::decode_literal(
    std::uint8_t const* data,
    std::size_t length,
    std::size_t& consumed,
    bool indexing,
    bool sensitive,
    decode_handler& handler,
    error_code& ec)
{
    consumed = 0;

    // Determine prefix bits based on type
    std::uint8_t prefix_bits;
    if(indexing)
        prefix_bits = 6;  // RFC 7541 Section 6.2.1
    else
        prefix_bits = 4;  // RFC 7541 Section 6.2.2 / 6.2.3

    std::uint64_t index;
    std::size_t int_consumed;
    decode_integer(data, length, index, prefix_bits, int_consumed, ec);
    if(ec)
        return;
    consumed += int_consumed;

    std::string name;
    if(index == 0)
    {
        // New name
        std::size_t str_consumed;
        decode_string(name, data + consumed, length - consumed,
            str_consumed, ec);
        if(ec)
            return;
        consumed += str_consumed;
    }
    else
    {
        // Indexed name
        auto entry = lookup(static_cast<std::size_t>(index), ec);
        if(ec)
            return;
        name.assign(entry.name.data(), entry.name.size());
    }

    // Decode value
    std::string value;
    std::size_t str_consumed;
    decode_string(value, data + consumed, length - consumed,
        str_consumed, ec);
    if(ec)
        return;
    consumed += str_consumed;

    if(indexing)
        dyn_table_.insert(name, value);

    handler.on_header(name, value, sensitive, ec);
}

void
decoder::decode_table_size_update(
    std::uint8_t const* data,
    std::size_t length,
    std::size_t& consumed,
    error_code& ec)
{
    // RFC 7541 Section 6.3: Dynamic Table Size Update
    // Starts with 3-bit pattern '001'
    std::uint64_t new_size;
    decode_integer(data, length, new_size, 5, consumed, ec);
    if(ec)
        return;

    dyn_table_.set_max_size(static_cast<std::size_t>(new_size));
}

void
decoder::decode(
    std::uint8_t const* data,
    std::size_t length,
    decode_handler& handler,
    error_code& ec)
{
    ec = {};
    std::size_t pos = 0;

    while(pos < length)
    {
        std::uint8_t byte = data[pos];
        std::size_t consumed = 0;

        if(byte & 0x80)
        {
            // Indexed Header Field (Section 6.1)
            decode_indexed(data + pos, length - pos,
                consumed, handler, ec);
        }
        else if(byte & 0x40)
        {
            // Literal with Incremental Indexing (Section 6.2.1)
            decode_literal(data + pos, length - pos,
                consumed, true, false, handler, ec);
        }
        else if(byte & 0x20)
        {
            // Dynamic Table Size Update (Section 6.3)
            decode_table_size_update(data + pos, length - pos,
                consumed, ec);
        }
        else if(byte & 0x10)
        {
            // Literal Never Indexed (Section 6.2.3)
            decode_literal(data + pos, length - pos,
                consumed, false, true, handler, ec);
        }
        else
        {
            // Literal Without Indexing (Section 6.2.2)
            decode_literal(data + pos, length - pos,
                consumed, false, false, handler, ec);
        }

        if(ec)
            return;

        pos += consumed;
    }
}

} // hpack
} // http2
} // beast
} // boost

#endif
