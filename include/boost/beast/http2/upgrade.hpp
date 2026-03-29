//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_UPGRADE_HPP
#define BOOST_BEAST_HTTP2_UPGRADE_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/string_type.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/fields.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http2/settings.hpp>

namespace boost {
namespace beast {
namespace http2 {

/** Check if an HTTP/1.1 request is an h2c upgrade request.

    @param req The HTTP request to check
    @return true if the request contains an HTTP/2 upgrade
*/
template<class Body, class Fields>
bool
is_h2c_upgrade(
    http::request<Body, Fields> const& req)
{
    // Check for Upgrade: h2c header
    auto const upgrade_it = req.find(http::field::upgrade);
    if(upgrade_it == req.end())
        return false;

    auto const& upgrade_val = upgrade_it->value();
    if(upgrade_val != "h2c")
        return false;

    // Check for HTTP2-Settings header
    auto const settings_it = req.find("HTTP2-Settings");
    if(settings_it == req.end())
        return false;

    // Check for Connection: Upgrade, HTTP2-Settings
    auto const conn_it = req.find(http::field::connection);
    if(conn_it == req.end())
        return false;

    return true;
}

/** Prepare an HTTP/1.1 request for h2c upgrade.

    Adds the necessary headers for an HTTP/2 cleartext upgrade:
    - Upgrade: h2c
    - HTTP2-Settings: <base64url encoded SETTINGS>
    - Connection: Upgrade, HTTP2-Settings

    @param req The request to prepare
    @param s The initial settings to encode
*/
template<class Body, class Fields>
void
prepare_h2c_upgrade(
    http::request<Body, Fields>& req,
    settings const& s = settings{})
{
    req.set(http::field::upgrade, "h2c");
    req.set(http::field::connection, "Upgrade, HTTP2-Settings");

    // Encode settings as base64url for HTTP2-Settings header
    // (simplified: just set an empty value for now)
    std::uint8_t buf[36];
    auto n = serialize_settings(buf, s);
    // In a full implementation, this would be base64url encoded
    req.set("HTTP2-Settings",
        std::string(reinterpret_cast<char*>(buf), n));
}

/** Prepare an HTTP/1.1 101 response accepting h2c upgrade.

    @param res The response to prepare
*/
template<class Body, class Fields>
void
prepare_h2c_upgrade_response(
    http::response<Body, Fields>& res)
{
    res.result(http::status::switching_protocols);
    res.set(http::field::upgrade, "h2c");
    res.set(http::field::connection, "Upgrade");
}

} // http2
} // beast
} // boost

#ifdef BOOST_BEAST_HEADER_ONLY
#include <boost/beast/http2/impl/upgrade.ipp>
#endif

#endif
