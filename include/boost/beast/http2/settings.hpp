//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_SETTINGS_HPP
#define BOOST_BEAST_HTTP2_SETTINGS_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/http2/error.hpp>
#include <boost/beast/http2/frame.hpp>
#include <cstddef>
#include <cstdint>

namespace boost {
namespace beast {
namespace http2 {

/// HTTP/2 settings identifiers (RFC 9113 Section 6.5.2)
enum class setting_id : std::uint16_t
{
    header_table_size       = 0x01,
    enable_push             = 0x02,
    max_concurrent_streams  = 0x03,
    initial_window_size     = 0x04,
    max_frame_size          = 0x05,
    max_header_list_size    = 0x06
};

/// A single setting parameter (identifier + value pair)
struct setting_parameter
{
    setting_id id;
    std::uint32_t value;
};

/** HTTP/2 connection settings (RFC 9113 Section 6.5.2)

    Holds the values for the six defined HTTP/2 settings parameters.
    Default values are per RFC 9113 Section 6.5.2.
*/
struct settings
{
    /// Maximum size of the HPACK dynamic table
    std::uint32_t header_table_size = 4096;

    /// Whether server push is enabled
    bool enable_push = true;

    /// Maximum number of concurrent streams
    std::uint32_t max_concurrent_streams = 100;

    /// Initial flow-control window size
    std::uint32_t initial_window_size = 65535;

    /// Maximum frame payload size
    std::uint32_t max_frame_size = 16384;

    /// Maximum header list size
    std::uint32_t max_header_list_size = 8192;
};

/** Serialize settings parameters into a buffer.

    Each parameter is encoded as 6 bytes (2-byte ID + 4-byte value)
    in network byte order.

    @param dest Pointer to writable memory (needs 6 * count bytes)
    @param params Pointer to array of setting parameters
    @param count Number of parameters to serialize
    @return Number of bytes written
*/
BOOST_BEAST_DECL
std::size_t
serialize_settings(
    std::uint8_t* dest,
    setting_parameter const* params,
    std::size_t count);

/** Serialize all non-default settings from a settings struct.

    @param dest Pointer to writable memory (needs up to 36 bytes)
    @param s The settings to serialize
    @return Number of bytes written
*/
BOOST_BEAST_DECL
std::size_t
serialize_settings(
    std::uint8_t* dest,
    settings const& s);

/** Parse settings parameters from a buffer.

    @param s The settings struct to populate
    @param src Pointer to the data
    @param length Length of the data (must be a multiple of 6)
    @param ec Set to the error, if any occurred
*/
BOOST_BEAST_DECL
void
parse_settings(
    settings& s,
    std::uint8_t const* src,
    std::size_t length,
    error_code& ec);

/** Validate a settings struct.

    Checks that all values are within the ranges specified
    by RFC 9113 Section 6.5.2.

    @param s The settings to validate
    @param ec Set to the error, if any occurred
*/
BOOST_BEAST_DECL
void
validate_settings(
    settings const& s,
    error_code& ec);

} // http2
} // beast
} // boost

#ifdef BOOST_BEAST_HEADER_ONLY
#include <boost/beast/http2/impl/settings.ipp>
#endif

#endif
