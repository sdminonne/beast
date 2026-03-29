//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_HPACK_IMPL_DYNAMIC_TABLE_IPP
#define BOOST_BEAST_HTTP2_HPACK_IMPL_DYNAMIC_TABLE_IPP

#include <boost/beast/http2/hpack/dynamic_table.hpp>

namespace boost {
namespace beast {
namespace http2 {
namespace hpack {

void
dynamic_table::insert(string_view name, string_view value)
{
    auto const sz = entry_size(name, value);

    // If the entry is larger than the max size, clear the table
    if(sz > max_size_)
    {
        clear();
        return;
    }

    // Evict until there's room
    while(current_size_ + sz > max_size_)
        evict();

    // Grow the backing vector if needed
    if(count_ == entries_.size())
        entries_.resize(entries_.empty() ? 16 : entries_.size() * 2);

    // Insert at insert_idx_ (circular)
    if(insert_idx_ == 0)
        insert_idx_ = entries_.size();
    --insert_idx_;

    entries_[insert_idx_].name.assign(name.data(), name.size());
    entries_[insert_idx_].value.assign(value.data(), value.size());
    ++count_;
    current_size_ += sz;
}

table_entry
dynamic_table::at(std::size_t index) const
{
    auto const real_idx = (insert_idx_ + index) % entries_.size();
    return {entries_[real_idx].name, entries_[real_idx].value};
}

void
dynamic_table::set_max_size(std::size_t new_max)
{
    max_size_ = new_max;
    while(current_size_ > max_size_)
        evict();
}

std::size_t
dynamic_table::find(
    string_view name,
    string_view value,
    bool& name_match) const
{
    name_match = false;
    std::size_t name_idx = static_cast<std::size_t>(-1);

    for(std::size_t i = 0; i < count_; ++i)
    {
        auto const real_idx = (insert_idx_ + i) % entries_.size();
        auto const& entry = entries_[real_idx];
        if(string_view(entry.name) == name)
        {
            if(string_view(entry.value) == value)
                return i;
            if(name_idx == static_cast<std::size_t>(-1))
            {
                name_idx = i;
                name_match = true;
            }
        }
    }

    return name_idx;
}

void
dynamic_table::clear()
{
    entries_.clear();
    insert_idx_ = 0;
    count_ = 0;
    current_size_ = 0;
}

void
dynamic_table::evict()
{
    if(count_ == 0)
        return;

    // Remove the oldest entry (at the end of the circular buffer)
    auto const oldest_idx = (insert_idx_ + count_ - 1) % entries_.size();
    auto const& entry = entries_[oldest_idx];
    current_size_ -= entry_size(entry.name, entry.value);
    --count_;
}

} // hpack
} // http2
} // beast
} // boost

#endif
