//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_HPACK_IMPL_INTEGER_IPP
#define BOOST_BEAST_HTTP2_HPACK_IMPL_INTEGER_IPP

#include <boost/beast/http2/hpack/integer.hpp>
#include <boost/beast/http2/error.hpp>

namespace boost {
namespace beast {
namespace http2 {
namespace hpack {

std::size_t
encode_integer(
    std::uint8_t* dest,
    std::size_t dest_size,
    std::uint64_t value,
    std::uint8_t prefix_bits,
    std::uint8_t pattern)
{
    std::uint8_t const max_prefix =
        static_cast<std::uint8_t>((1u << prefix_bits) - 1u);

    if(value < max_prefix)
    {
        if(dest_size < 1)
            return 0;
        dest[0] = static_cast<std::uint8_t>(
            pattern | (value & max_prefix));
        return 1;
    }

    if(dest_size < 1)
        return 0;
    dest[0] = static_cast<std::uint8_t>(pattern | max_prefix);
    value -= max_prefix;
    std::size_t n = 1;

    while(value >= 128)
    {
        if(n >= dest_size)
            return 0;
        dest[n++] = static_cast<std::uint8_t>(
            (value & 0x7f) | 0x80);
        value >>= 7;
    }

    if(n >= dest_size)
        return 0;
    dest[n++] = static_cast<std::uint8_t>(value);
    return n;
}

void
decode_integer(
    std::uint8_t const* src,
    std::size_t length,
    std::uint64_t& value,
    std::uint8_t prefix_bits,
    std::size_t& consumed,
    error_code& ec)
{
    ec = {};
    consumed = 0;

    if(length == 0)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::need_more);
        return;
    }

    std::uint8_t const max_prefix =
        static_cast<std::uint8_t>((1u << prefix_bits) - 1u);

    value = src[0] & max_prefix;
    consumed = 1;

    if(value < max_prefix)
        return;

    std::uint64_t m = 0;
    for(;;)
    {
        if(consumed >= length)
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::need_more);
            return;
        }

        std::uint8_t b = src[consumed++];
        // Check for overflow: m must be < 64, and the shifted value
        // must not overflow uint64_t
        if(m >= 63 && (b & 0x7f) > 1)
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::hpack_integer_overflow);
            return;
        }

        value += static_cast<std::uint64_t>(b & 0x7f) << m;
        m += 7;

        if((b & 0x80) == 0)
            break;
    }
}

} // hpack
} // http2
} // beast
} // boost

#endif
