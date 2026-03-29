//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_MESSAGE_ADAPTER_HPP
#define BOOST_BEAST_HTTP2_MESSAGE_ADAPTER_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/string_type.hpp>
#include <boost/beast/http2/error.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/fields.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <string>
#include <vector>
#include <utility>

namespace boost {
namespace beast {
namespace http2 {

/// A header field name-value pair for HTTP/2 header lists
using header_field = std::pair<std::string, std::string>;

/// A list of header fields for HTTP/2
using header_list = std::vector<header_field>;

/** Convert an HTTP request message to an HTTP/2 header list.

    Extracts pseudo-headers (:method, :scheme, :authority, :path)
    and regular headers from the request message.

    @param msg The HTTP request message
    @param out The output header list
*/
template<class Body, class Fields>
void
request_to_header_list(
    http::request<Body, Fields> const& msg,
    header_list& out)
{
    out.clear();

    // Pseudo-headers (must come first)
    auto const method = msg.method_string();
    out.emplace_back(":method",
        std::string(method.data(), method.size()));

    // Skip :scheme and :path for CONNECT method
    if(msg.method() != http::verb::connect)
    {
        // Determine scheme from target or default to "https"
        out.emplace_back(":scheme", "https");

        auto const target = msg.target();
        out.emplace_back(":path",
            std::string(target.data(), target.size()));
    }

    // :authority from Host header or target
    auto const host_it = msg.find(http::field::host);
    if(host_it != msg.end())
    {
        auto const& host_val = host_it->value();
        out.emplace_back(":authority",
            std::string(host_val.data(), host_val.size()));
    }

    // Regular headers (skip pseudo-headers and connection-specific)
    for(auto const& field : msg)
    {
        auto const name = field.name_string();
        // Skip connection-specific headers (RFC 9113 Section 8.2.2)
        if(name == "connection" ||
           name == "keep-alive" ||
           name == "proxy-connection" ||
           name == "transfer-encoding" ||
           name == "upgrade" ||
           name == "host")  // :authority replaces Host
            continue;

        out.emplace_back(
            std::string(name.data(), name.size()),
            std::string(field.value().data(), field.value().size()));
    }
}

/** Convert an HTTP response message to an HTTP/2 header list.

    Extracts the :status pseudo-header and regular headers
    from the response message.

    @param msg The HTTP response message
    @param out The output header list
*/
template<class Body, class Fields>
void
response_to_header_list(
    http::response<Body, Fields> const& msg,
    header_list& out)
{
    out.clear();

    // :status pseudo-header
    out.emplace_back(":status",
        std::to_string(static_cast<unsigned>(msg.result_int())));

    // Regular headers
    for(auto const& field : msg)
    {
        auto const name = field.name_string();
        // Skip connection-specific headers
        if(name == "connection" ||
           name == "keep-alive" ||
           name == "proxy-connection" ||
           name == "transfer-encoding" ||
           name == "upgrade")
            continue;

        out.emplace_back(
            std::string(name.data(), name.size()),
            std::string(field.value().data(), field.value().size()));
    }
}

/** Populate an HTTP request from a decoded HTTP/2 header list.

    Maps pseudo-headers back to the appropriate request fields:
    :method -> method, :path -> target, :authority -> Host header

    @param headers The decoded header list
    @param msg The request message to populate
    @param ec Set to the error, if any occurred
*/
template<class Body, class Fields>
void
header_list_to_request(
    header_list const& headers,
    http::request<Body, Fields>& msg,
    error_code& ec)
{
    ec = {};
    bool has_method = false;
    bool has_path = false;

    for(auto const& h : headers)
    {
        if(h.first == ":method")
        {
            msg.method_string(h.second);
            has_method = true;
        }
        else if(h.first == ":path")
        {
            msg.target(h.second);
            has_path = true;
        }
        else if(h.first == ":scheme")
        {
            // Stored for informational purposes
            // The scheme is implicit from the transport
        }
        else if(h.first == ":authority")
        {
            msg.set(http::field::host, h.second);
        }
        else if(h.first[0] == ':')
        {
            // Unknown pseudo-header
            BOOST_BEAST_ASSIGN_EC(ec, error::bad_pseudo_header);
            return;
        }
        else
        {
            msg.set(h.first, h.second);
        }
    }

    if(!has_method)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::bad_pseudo_header);
        return;
    }

    // :path is required except for CONNECT
    if(!has_path && msg.method() != http::verb::connect)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::bad_pseudo_header);
        return;
    }

    // Set HTTP version to 2.0 (represented as 20 in Beast)
    msg.version(20);
}

/** Populate an HTTP response from a decoded HTTP/2 header list.

    Maps the :status pseudo-header to the response status.

    @param headers The decoded header list
    @param msg The response message to populate
    @param ec Set to the error, if any occurred
*/
template<class Body, class Fields>
void
header_list_to_response(
    header_list const& headers,
    http::response<Body, Fields>& msg,
    error_code& ec)
{
    ec = {};
    bool has_status = false;

    for(auto const& h : headers)
    {
        if(h.first == ":status")
        {
            unsigned status_code = 0;
            for(char c : h.second)
            {
                if(c < '0' || c > '9')
                {
                    BOOST_BEAST_ASSIGN_EC(ec, error::bad_pseudo_header);
                    return;
                }
                status_code = status_code * 10 + (c - '0');
            }
            msg.result(static_cast<http::status>(status_code));
            has_status = true;
        }
        else if(h.first[0] == ':')
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::bad_pseudo_header);
            return;
        }
        else
        {
            msg.set(h.first, h.second);
        }
    }

    if(!has_status)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::bad_pseudo_header);
        return;
    }

    msg.version(20);
}

} // http2
} // beast
} // boost

#endif
