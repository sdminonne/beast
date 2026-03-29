//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_HPACK_IMPL_STATIC_TABLE_IPP
#define BOOST_BEAST_HTTP2_HPACK_IMPL_STATIC_TABLE_IPP

#include <boost/beast/http2/hpack/static_table.hpp>

namespace boost {
namespace beast {
namespace http2 {
namespace hpack {

// RFC 7541 Appendix A - Static Table Definition
table_entry const&
static_table_entry(std::size_t index)
{
    static constexpr table_entry table[] = {
        {"",                          ""},              // 0 (unused, 1-based)
        {":authority",                ""},              // 1
        {":method",                   "GET"},           // 2
        {":method",                   "POST"},          // 3
        {":path",                     "/"},             // 4
        {":path",                     "/index.html"},   // 5
        {":scheme",                   "http"},          // 6
        {":scheme",                   "https"},         // 7
        {":status",                   "200"},           // 8
        {":status",                   "204"},           // 9
        {":status",                   "206"},           // 10
        {":status",                   "304"},           // 11
        {":status",                   "400"},           // 12
        {":status",                   "404"},           // 13
        {":status",                   "500"},           // 14
        {"accept-charset",            ""},              // 15
        {"accept-encoding",           "gzip, deflate"}, // 16
        {"accept-language",           ""},              // 17
        {"accept-ranges",             ""},              // 18
        {"accept",                    ""},              // 19
        {"access-control-allow-origin",""},             // 20
        {"age",                       ""},              // 21
        {"allow",                     ""},              // 22
        {"authorization",             ""},              // 23
        {"cache-control",             ""},              // 24
        {"content-disposition",       ""},              // 25
        {"content-encoding",          ""},              // 26
        {"content-language",          ""},              // 27
        {"content-length",            ""},              // 28
        {"content-location",          ""},              // 29
        {"content-range",             ""},              // 30
        {"content-type",              ""},              // 31
        {"cookie",                    ""},              // 32
        {"date",                      ""},              // 33
        {"etag",                      ""},              // 34
        {"expect",                    ""},              // 35
        {"expires",                   ""},              // 36
        {"from",                      ""},              // 37
        {"host",                      ""},              // 38
        {"if-match",                  ""},              // 39
        {"if-modified-since",         ""},              // 40
        {"if-none-match",             ""},              // 41
        {"if-range",                  ""},              // 42
        {"if-unmodified-since",       ""},              // 43
        {"last-modified",             ""},              // 44
        {"link",                      ""},              // 45
        {"location",                  ""},              // 46
        {"max-forwards",              ""},              // 47
        {"proxy-authenticate",        ""},              // 48
        {"proxy-authorization",       ""},              // 49
        {"range",                     ""},              // 50
        {"referer",                   ""},              // 51
        {"refresh",                   ""},              // 52
        {"retry-after",               ""},              // 53
        {"server",                    ""},              // 54
        {"set-cookie",                ""},              // 55
        {"strict-transport-security", ""},              // 56
        {"transfer-encoding",         ""},              // 57
        {"user-agent",                ""},              // 58
        {"vary",                      ""},              // 59
        {"via",                       ""},              // 60
        {"www-authenticate",          ""},              // 61
    };
    return table[index];
}

std::size_t
static_table_find(
    string_view name,
    string_view value,
    bool& name_match)
{
    name_match = false;
    std::size_t name_idx = 0;

    for(std::size_t i = 1; i <= static_table_size; ++i)
    {
        auto const& entry = static_table_entry(i);
        if(entry.name == name)
        {
            if(entry.value == value)
                return i;
            if(name_idx == 0)
            {
                name_idx = i;
                name_match = true;
            }
        }
    }

    return name_idx;
}

} // hpack
} // http2
} // beast
} // boost

#endif
