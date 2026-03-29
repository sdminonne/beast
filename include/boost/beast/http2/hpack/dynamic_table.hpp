//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_HPACK_DYNAMIC_TABLE_HPP
#define BOOST_BEAST_HTTP2_HPACK_DYNAMIC_TABLE_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/string_type.hpp>
#include <boost/beast/http2/hpack/static_table.hpp>
#include <cstddef>
#include <string>
#include <vector>

namespace boost {
namespace beast {
namespace http2 {
namespace hpack {

/** HPACK dynamic table (RFC 7541 Section 2.3.2).

    A FIFO table of header field entries with a maximum size
    constraint. Entries are evicted from the oldest end when
    the table exceeds its maximum size.

    The overhead per entry is 32 bytes as specified in
    RFC 7541 Section 4.1.
*/
class dynamic_table
{
    // Each stored entry owns its name and value strings
    struct stored_entry
    {
        std::string name;
        std::string value;
    };

    std::vector<stored_entry> entries_;
    std::size_t insert_idx_ = 0;
    std::size_t count_ = 0;
    std::size_t current_size_ = 0;
    std::size_t max_size_ = 4096;

    static constexpr std::size_t entry_overhead = 32;

public:
    dynamic_table() = default;

    /** Insert a new entry at the beginning of the table.

        If the new entry causes the table to exceed its maximum
        size, entries are evicted from the end until the entry fits.

        @param name The header field name
        @param value The header field value
    */
    BOOST_BEAST_DECL
    void
    insert(string_view name, string_view value);

    /** Get an entry from the dynamic table.

        Index 0 is the most recently inserted entry.

        @param index 0-based index into the dynamic table
        @return The table entry
    */
    BOOST_BEAST_DECL
    table_entry
    at(std::size_t index) const;

    /** Set the maximum size of the dynamic table.

        If the new maximum is smaller, entries are evicted
        as needed.

        @param new_max The new maximum size in bytes
    */
    BOOST_BEAST_DECL
    void
    set_max_size(std::size_t new_max);

    /** Find an entry in the dynamic table.

        @param name The header field name to search for
        @param value The header field value to search for
        @param name_match Set to true if only the name matched
        @return The 0-based index, or static_cast<size_t>(-1) if not found
    */
    BOOST_BEAST_DECL
    std::size_t
    find(string_view name, string_view value, bool& name_match) const;

    /// Returns the number of entries in the table
    std::size_t
    size() const noexcept
    {
        return count_;
    }

    /// Returns the current size of the table in bytes
    std::size_t
    current_size() const noexcept
    {
        return current_size_;
    }

    /// Returns the maximum size of the table in bytes
    std::size_t
    max_size() const noexcept
    {
        return max_size_;
    }

    /// Clear all entries from the table
    BOOST_BEAST_DECL
    void
    clear();

private:
    BOOST_BEAST_DECL
    void
    evict();

    static
    std::size_t
    entry_size(string_view name, string_view value)
    {
        return name.size() + value.size() + entry_overhead;
    }
};

} // hpack
} // http2
} // beast
} // boost

#ifdef BOOST_BEAST_HEADER_ONLY
#include <boost/beast/http2/hpack/impl/dynamic_table.ipp>
#endif

#endif
