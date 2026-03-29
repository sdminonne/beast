//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_HPACK_STATIC_TABLE_HPP
#define BOOST_BEAST_HTTP2_HPACK_STATIC_TABLE_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/string_type.hpp>
#include <cstddef>

namespace boost {
namespace beast {
namespace http2 {
namespace hpack {

/// An entry in the HPACK header table
struct table_entry
{
    string_view name;
    string_view value;
};

/// Number of entries in the HPACK static table (RFC 7541 Appendix A)
constexpr std::size_t static_table_size = 61;

/** Get an entry from the HPACK static table (RFC 7541 Appendix A).

    @param index 1-based index (1 to 61)
    @return The table entry at the given index
*/
BOOST_BEAST_DECL
table_entry const&
static_table_entry(std::size_t index);

/** Find an entry in the HPACK static table.

    @param name The header field name to search for
    @param value The header field value to search for
    @param name_match Set to true if only the name matched
    @return The 1-based index, or 0 if not found
*/
BOOST_BEAST_DECL
std::size_t
static_table_find(
    string_view name,
    string_view value,
    bool& name_match);

} // hpack
} // http2
} // beast
} // boost

#ifdef BOOST_BEAST_HEADER_ONLY
#include <boost/beast/http2/hpack/impl/static_table.ipp>
#endif

#endif
