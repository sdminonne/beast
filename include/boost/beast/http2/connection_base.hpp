//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_CONNECTION_BASE_HPP
#define BOOST_BEAST_HTTP2_CONNECTION_BASE_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/http2/settings.hpp>
#include <chrono>
#include <cstdint>

namespace boost {
namespace beast {
namespace http2 {

/** Base class for HTTP/2 connection configuration.

    Provides types for configuring timeouts and connection options.
*/
class connection_base
{
public:
    /** Timeout configuration for the connection.
    */
    struct timeout
    {
        /// Time limit for the handshake
        std::chrono::steady_clock::duration handshake_timeout =
            std::chrono::seconds(30);

        /// Time limit for idle connections
        std::chrono::steady_clock::duration idle_timeout =
            std::chrono::seconds(300);

        /// Whether to send keep-alive PINGs
        bool keep_alive_pings = false;

        /// Suggested defaults for servers
        static timeout suggested_server()
        {
            timeout t;
            t.handshake_timeout = std::chrono::seconds(30);
            t.idle_timeout = std::chrono::seconds(300);
            t.keep_alive_pings = false;
            return t;
        }

        /// Suggested defaults for clients
        static timeout suggested_client()
        {
            timeout t;
            t.handshake_timeout = std::chrono::seconds(30);
            t.idle_timeout = std::chrono::seconds(300);
            t.keep_alive_pings = true;
            return t;
        }
    };

    /** Connection options.
    */
    struct options
    {
        /// Initial settings to send during handshake
        settings initial_settings;
    };

protected:
    connection_base() = default;
};

} // http2
} // beast
} // boost

#endif
