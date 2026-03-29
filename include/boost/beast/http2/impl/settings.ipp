//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_IMPL_SETTINGS_IPP
#define BOOST_BEAST_HTTP2_IMPL_SETTINGS_IPP

#include <boost/beast/http2/settings.hpp>
#include <boost/beast/http2/error.hpp>

namespace boost {
namespace beast {
namespace http2 {

namespace detail {

inline
void
put_uint16_be(std::uint8_t* dest, std::uint16_t v)
{
    dest[0] = static_cast<std::uint8_t>((v >> 8) & 0xff);
    dest[1] = static_cast<std::uint8_t>(v & 0xff);
}

inline
void
put_uint32_be(std::uint8_t* dest, std::uint32_t v)
{
    dest[0] = static_cast<std::uint8_t>((v >> 24) & 0xff);
    dest[1] = static_cast<std::uint8_t>((v >> 16) & 0xff);
    dest[2] = static_cast<std::uint8_t>((v >> 8) & 0xff);
    dest[3] = static_cast<std::uint8_t>(v & 0xff);
}

inline
std::uint16_t
get_uint16_be(std::uint8_t const* src)
{
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(src[0]) << 8) |
        static_cast<std::uint16_t>(src[1]));
}

inline
std::uint32_t
get_uint32_be(std::uint8_t const* src)
{
    return
        (static_cast<std::uint32_t>(src[0]) << 24) |
        (static_cast<std::uint32_t>(src[1]) << 16) |
        (static_cast<std::uint32_t>(src[2]) << 8) |
        static_cast<std::uint32_t>(src[3]);
}

} // detail

std::size_t
serialize_settings(
    std::uint8_t* dest,
    setting_parameter const* params,
    std::size_t count)
{
    for(std::size_t i = 0; i < count; ++i)
    {
        detail::put_uint16_be(dest, static_cast<std::uint16_t>(params[i].id));
        detail::put_uint32_be(dest + 2, params[i].value);
        dest += 6;
    }
    return count * 6;
}

std::size_t
serialize_settings(
    std::uint8_t* dest,
    settings const& s)
{
    settings const d; // defaults
    std::size_t n = 0;

    auto emit = [&](setting_id id, std::uint32_t value, std::uint32_t def)
    {
        if(value != def)
        {
            detail::put_uint16_be(dest + n, static_cast<std::uint16_t>(id));
            detail::put_uint32_be(dest + n + 2, value);
            n += 6;
        }
    };

    emit(setting_id::header_table_size, s.header_table_size, d.header_table_size);
    emit(setting_id::enable_push, s.enable_push ? 1u : 0u, d.enable_push ? 1u : 0u);
    emit(setting_id::max_concurrent_streams, s.max_concurrent_streams, d.max_concurrent_streams);
    emit(setting_id::initial_window_size, s.initial_window_size, d.initial_window_size);
    emit(setting_id::max_frame_size, s.max_frame_size, d.max_frame_size);
    emit(setting_id::max_header_list_size, s.max_header_list_size, d.max_header_list_size);

    return n;
}

void
parse_settings(
    settings& s,
    std::uint8_t const* src,
    std::size_t length,
    error_code& ec)
{
    ec = {};

    if(length % 6 != 0)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::frame_size_error);
        return;
    }

    for(std::size_t i = 0; i < length; i += 6)
    {
        auto id = detail::get_uint16_be(src + i);
        auto value = detail::get_uint32_be(src + i + 2);

        switch(static_cast<setting_id>(id))
        {
        case setting_id::header_table_size:
            s.header_table_size = value;
            break;

        case setting_id::enable_push:
            if(value > 1)
            {
                BOOST_BEAST_ASSIGN_EC(ec, error::protocol_error);
                return;
            }
            s.enable_push = (value != 0);
            break;

        case setting_id::max_concurrent_streams:
            s.max_concurrent_streams = value;
            break;

        case setting_id::initial_window_size:
            if(value > static_cast<std::uint32_t>(max_window_size))
            {
                BOOST_BEAST_ASSIGN_EC(ec, error::flow_control_error);
                return;
            }
            s.initial_window_size = value;
            break;

        case setting_id::max_frame_size:
            if(value < default_max_frame_size || value > max_allowed_frame_size)
            {
                BOOST_BEAST_ASSIGN_EC(ec, error::protocol_error);
                return;
            }
            s.max_frame_size = value;
            break;

        case setting_id::max_header_list_size:
            s.max_header_list_size = value;
            break;

        default:
            // Unknown settings MUST be ignored (RFC 9113 Section 6.5.2)
            break;
        }
    }
}

void
validate_settings(
    settings const& s,
    error_code& ec)
{
    ec = {};

    if(s.enable_push != false && s.enable_push != true)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::protocol_error);
        return;
    }

    if(s.initial_window_size > static_cast<std::uint32_t>(max_window_size))
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::flow_control_error);
        return;
    }

    if(s.max_frame_size < default_max_frame_size ||
       s.max_frame_size > max_allowed_frame_size)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::protocol_error);
        return;
    }
}

} // http2
} // beast
} // boost

#endif
