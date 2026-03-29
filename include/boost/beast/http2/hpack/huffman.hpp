//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_HPACK_HUFFMAN_HPP
#define BOOST_BEAST_HTTP2_HPACK_HUFFMAN_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/error.hpp>
#include <cstddef>
#include <cstdint>
#include <string>

namespace boost {
namespace beast {
namespace http2 {
namespace hpack {

/** Encode a string using HPACK Huffman coding (RFC 7541 Appendix B).

    @param dest Pointer to writable memory
    @param dest_size Available bytes in dest
    @param src Pointer to source string data
    @param src_length Length of source string
    @return Number of bytes written, or 0 if dest is too small
*/
BOOST_BEAST_DECL
std::size_t
huffman_encode(
    std::uint8_t* dest,
    std::size_t dest_size,
    std::uint8_t const* src,
    std::size_t src_length);

/** Calculate the encoded size of a string using HPACK Huffman coding.

    @param src Pointer to source string data
    @param src_length Length of source string
    @return Number of bytes the encoded string would occupy
*/
BOOST_BEAST_DECL
std::size_t
huffman_encoded_size(
    std::uint8_t const* src,
    std::size_t src_length);

/** Decode an HPACK Huffman-encoded string (RFC 7541 Appendix B).

    @param dest Output string
    @param src Pointer to encoded data
    @param src_length Length of encoded data
    @param ec Set to the error, if any occurred
*/
BOOST_BEAST_DECL
void
huffman_decode(
    std::string& dest,
    std::uint8_t const* src,
    std::size_t src_length,
    error_code& ec);

} // hpack
} // http2
} // beast
} // boost

#ifdef BOOST_BEAST_HEADER_ONLY
#include <boost/beast/http2/hpack/impl/huffman.ipp>
#endif

#endif
