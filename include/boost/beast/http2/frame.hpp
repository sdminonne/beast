//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_FRAME_HPP
#define BOOST_BEAST_HTTP2_FRAME_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/error.hpp>
#include <cstddef>
#include <cstdint>

namespace boost {
namespace beast {
namespace http2 {

/// Size of the HTTP/2 frame header in bytes
constexpr std::size_t frame_header_size = 9;

/// Default maximum frame payload size (RFC 9113 Section 6.5.2)
constexpr std::uint32_t default_max_frame_size = 16384;

/// Maximum allowed frame payload size (RFC 9113 Section 4.2)
constexpr std::uint32_t max_allowed_frame_size = 16777215;

/// Default initial window size (RFC 9113 Section 6.9.2)
constexpr std::int32_t default_initial_window_size = 65535;

/// Maximum window size (RFC 9113 Section 6.9.1)
constexpr std::int32_t max_window_size = 2147483647;

/// HTTP/2 frame types (RFC 9113 Section 6)
enum class frame_type : std::uint8_t
{
    data            = 0x00,
    headers         = 0x01,
    priority        = 0x02,
    rst_stream      = 0x03,
    settings        = 0x04,
    push_promise    = 0x05,
    ping            = 0x06,
    goaway          = 0x07,
    window_update   = 0x08,
    continuation    = 0x09
};

/// HTTP/2 frame flags (RFC 9113 Section 6)
namespace frame_flag {

/// DATA, HEADERS: indicates end of stream
constexpr std::uint8_t end_stream   = 0x01;

/// SETTINGS, PING: indicates acknowledgement
constexpr std::uint8_t ack          = 0x01;

/// HEADERS, PUSH_PROMISE: indicates end of headers
constexpr std::uint8_t end_headers  = 0x04;

/// DATA, HEADERS: indicates padding present
constexpr std::uint8_t padded       = 0x08;

/// HEADERS: indicates priority information present
constexpr std::uint8_t priority     = 0x20;

} // frame_flag

/** HTTP/2 frame header (RFC 9113 Section 4.1)

    The 9-byte header that precedes every HTTP/2 frame:
    - 24-bit length
    - 8-bit type
    - 8-bit flags
    - 1-bit reserved + 31-bit stream identifier
*/
struct frame_header
{
    /// Length of the frame payload
    std::uint32_t length = 0;

    /// Frame type
    frame_type type = frame_type::data;

    /// Frame flags
    std::uint8_t flags = 0;

    /// Stream identifier (31 bits, R bit masked)
    std::uint32_t stream_id = 0;
};

/** Serialize a frame header to a 9-byte buffer.

    @param dest Pointer to at least 9 bytes of writable memory
    @param fh The frame header to serialize
*/
BOOST_BEAST_DECL
void
serialize_frame_header(
    std::uint8_t* dest,
    frame_header const& fh);

/** Parse a frame header from a 9-byte buffer.

    @param fh The frame header to populate
    @param src Pointer to at least 9 bytes of data
    @param ec Set to the error, if any occurred
*/
BOOST_BEAST_DECL
void
parse_frame_header(
    frame_header& fh,
    std::uint8_t const* src,
    error_code& ec);

} // http2
} // beast
} // boost

#ifdef BOOST_BEAST_HEADER_ONLY
#include <boost/beast/http2/impl/frame.ipp>
#endif

#endif
