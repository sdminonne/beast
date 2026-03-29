//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_HPACK_DECODER_HPP
#define BOOST_BEAST_HTTP2_HPACK_DECODER_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/string_type.hpp>
#include <boost/beast/http2/hpack/dynamic_table.hpp>
#include <cstddef>
#include <cstdint>

namespace boost {
namespace beast {
namespace http2 {
namespace hpack {

/** Callback interface for decoded header fields.

    Users implement this interface to receive decoded headers
    from the HPACK decoder.
*/
struct decode_handler
{
    virtual ~decode_handler() = default;

    /** Called for each decoded header field.

        @param name The header field name
        @param value The header field value
        @param sensitive Whether the field was marked as never-indexed
        @param ec Set to abort decoding with an error
    */
    virtual void
    on_header(
        string_view name,
        string_view value,
        bool sensitive,
        error_code& ec) = 0;
};

/** HPACK header block decoder (RFC 7541).

    Decodes HPACK-encoded header blocks into individual header
    field name-value pairs. Maintains the dynamic table state.
*/
class decoder
{
    dynamic_table dyn_table_;

public:
    decoder() = default;

    /** Decode a complete header block.

        @param data Pointer to the encoded header block
        @param length Length of the encoded header block
        @param handler Callback to receive decoded headers
        @param ec Set to the error, if any occurred
    */
    BOOST_BEAST_DECL
    void
    decode(
        std::uint8_t const* data,
        std::size_t length,
        decode_handler& handler,
        error_code& ec);

    /** Set the maximum dynamic table size.

        @param size The new maximum size
    */
    void
    set_max_table_size(std::size_t size)
    {
        dyn_table_.set_max_size(size);
    }

    /// Access the dynamic table (for testing)
    dynamic_table const&
    table() const noexcept
    {
        return dyn_table_;
    }

private:
    BOOST_BEAST_DECL
    void
    decode_indexed(
        std::uint8_t const* data,
        std::size_t length,
        std::size_t& consumed,
        decode_handler& handler,
        error_code& ec);

    BOOST_BEAST_DECL
    void
    decode_literal(
        std::uint8_t const* data,
        std::size_t length,
        std::size_t& consumed,
        bool indexing,
        bool sensitive,
        decode_handler& handler,
        error_code& ec);

    BOOST_BEAST_DECL
    void
    decode_table_size_update(
        std::uint8_t const* data,
        std::size_t length,
        std::size_t& consumed,
        error_code& ec);

    BOOST_BEAST_DECL
    void
    decode_string(
        std::string& out,
        std::uint8_t const* data,
        std::size_t length,
        std::size_t& consumed,
        error_code& ec);

    BOOST_BEAST_DECL
    table_entry
    lookup(std::size_t index, error_code& ec) const;
};

} // hpack
} // http2
} // beast
} // boost

#ifdef BOOST_BEAST_HEADER_ONLY
#include <boost/beast/http2/hpack/impl/decoder.ipp>
#endif

#endif
