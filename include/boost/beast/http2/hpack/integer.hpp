//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_HPACK_INTEGER_HPP
#define BOOST_BEAST_HTTP2_HPACK_INTEGER_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/http2/error.hpp>
#include <cstddef>
#include <cstdint>

namespace boost {
namespace beast {
namespace http2 {
namespace hpack {

/** Encode an HPACK integer (RFC 7541 Section 5.1).

    @param dest Pointer to writable memory
    @param dest_size Available bytes in dest
    @param value The integer value to encode
    @param prefix_bits Number of prefix bits (1-8)
    @param pattern Bits to OR into the first byte above the prefix
    @return Number of bytes written
*/
BOOST_BEAST_DECL
std::size_t
encode_integer(
    std::uint8_t* dest,
    std::size_t dest_size,
    std::uint64_t value,
    std::uint8_t prefix_bits,
    std::uint8_t pattern);

/** Decode an HPACK integer (RFC 7541 Section 5.1).

    @param src Pointer to encoded data
    @param length Length of encoded data
    @param value The decoded integer value
    @param prefix_bits Number of prefix bits (1-8)
    @param consumed Number of bytes consumed from src
    @param ec Set to the error, if any occurred
*/
BOOST_BEAST_DECL
void
decode_integer(
    std::uint8_t const* src,
    std::size_t length,
    std::uint64_t& value,
    std::uint8_t prefix_bits,
    std::size_t& consumed,
    error_code& ec);

} // hpack
} // http2
} // beast
} // boost

#ifdef BOOST_BEAST_HEADER_ONLY
#include <boost/beast/http2/hpack/impl/integer.ipp>
#endif

#endif
