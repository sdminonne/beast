//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_HPACK_ENCODER_HPP
#define BOOST_BEAST_HTTP2_HPACK_ENCODER_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/string_type.hpp>
#include <boost/beast/http2/hpack/dynamic_table.hpp>
#include <cstddef>
#include <cstdint>

namespace boost {
namespace beast {
namespace http2 {
namespace hpack {

/** HPACK header block encoder (RFC 7541).

    Encodes header field name-value pairs into HPACK-encoded
    header blocks. Maintains the dynamic table state.
*/
class encoder
{
    dynamic_table dyn_table_;

public:
    encoder() = default;

    /** Encode a header field.

        @param dest Pointer to writable memory
        @param dest_size Available bytes in dest
        @param name The header field name
        @param value The header field value
        @param indexing Whether to add the field to the dynamic table
        @param sensitive Whether the field should never be indexed
        @return Number of bytes written
    */
    BOOST_BEAST_DECL
    std::size_t
    encode_field(
        std::uint8_t* dest,
        std::size_t dest_size,
        string_view name,
        string_view value,
        bool indexing = true,
        bool sensitive = false);

    /** Encode a dynamic table size update.

        @param dest Pointer to writable memory
        @param dest_size Available bytes in dest
        @param new_size The new maximum table size
        @return Number of bytes written
    */
    BOOST_BEAST_DECL
    std::size_t
    encode_table_size_update(
        std::uint8_t* dest,
        std::size_t dest_size,
        std::size_t new_size);

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
    std::size_t
    encode_string(
        std::uint8_t* dest,
        std::size_t dest_size,
        string_view str);
};

} // hpack
} // http2
} // beast
} // boost

#ifdef BOOST_BEAST_HEADER_ONLY
#include <boost/beast/http2/hpack/impl/encoder.ipp>
#endif

#endif
