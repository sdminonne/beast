//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_IMPL_FRAME_IPP
#define BOOST_BEAST_HTTP2_IMPL_FRAME_IPP

#include <boost/beast/http2/frame.hpp>
#include <boost/beast/http2/error.hpp>

namespace boost {
namespace beast {
namespace http2 {

void
serialize_frame_header(
    std::uint8_t* dest,
    frame_header const& fh)
{
    // 24-bit length (big-endian)
    dest[0] = static_cast<std::uint8_t>((fh.length >> 16) & 0xff);
    dest[1] = static_cast<std::uint8_t>((fh.length >> 8) & 0xff);
    dest[2] = static_cast<std::uint8_t>(fh.length & 0xff);

    // 8-bit type
    dest[3] = static_cast<std::uint8_t>(fh.type);

    // 8-bit flags
    dest[4] = fh.flags;

    // 1-bit reserved (0) + 31-bit stream id (big-endian)
    std::uint32_t sid = fh.stream_id & 0x7fffffffu;
    dest[5] = static_cast<std::uint8_t>((sid >> 24) & 0xff);
    dest[6] = static_cast<std::uint8_t>((sid >> 16) & 0xff);
    dest[7] = static_cast<std::uint8_t>((sid >> 8) & 0xff);
    dest[8] = static_cast<std::uint8_t>(sid & 0xff);
}

void
parse_frame_header(
    frame_header& fh,
    std::uint8_t const* src,
    error_code& ec)
{
    ec = {};

    // 24-bit length (big-endian)
    fh.length =
        (static_cast<std::uint32_t>(src[0]) << 16) |
        (static_cast<std::uint32_t>(src[1]) << 8) |
        static_cast<std::uint32_t>(src[2]);

    // 8-bit type
    std::uint8_t raw_type = src[3];
    if(raw_type > static_cast<std::uint8_t>(frame_type::continuation))
    {
        // Unknown frame types must be ignored per RFC 9113 Section 4.1,
        // but we still parse them. The caller can decide.
    }
    fh.type = static_cast<frame_type>(raw_type);

    // 8-bit flags
    fh.flags = src[4];

    // 1-bit reserved + 31-bit stream id (big-endian)
    fh.stream_id =
        (static_cast<std::uint32_t>(src[5] & 0x7f) << 24) |
        (static_cast<std::uint32_t>(src[6]) << 16) |
        (static_cast<std::uint32_t>(src[7]) << 8) |
        static_cast<std::uint32_t>(src[8]);
}

} // http2
} // beast
} // boost

#endif
