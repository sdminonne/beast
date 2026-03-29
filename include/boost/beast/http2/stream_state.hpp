//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_STREAM_STATE_HPP
#define BOOST_BEAST_HTTP2_STREAM_STATE_HPP

#include <boost/beast/core/detail/config.hpp>

namespace boost {
namespace beast {
namespace http2 {

/** HTTP/2 stream states (RFC 9113 Section 5.1).

    Each stream progresses through a defined lifecycle:

    @code
                        +--------+
                  PP /  | idle   |\ PP
                   /    |        | \
                  v     +--------+  v
           +----------+          +----------+
           | reserved |          | reserved |
           | (local)  |          | (remote) |
           +----------+          +----------+
                |                      |
                | H / ES              | H / ES
                |                      |
                v                      v
           +----------+          +----------+
           | half-    |          | half-    |
           | closed   |          | closed   |
           | (remote) |          | (local)  |
           +----------+          +----------+
                |                      |
                | ES / R              | ES / R
                |                      |
                v                      v
                +--------+
                | closed |
                +--------+
    @endcode

    H: HEADERS, PP: PUSH_PROMISE, ES: END_STREAM, R: RST_STREAM
*/
enum class stream_state
{
    /// Stream is idle (initial state)
    idle,

    /// Stream reserved by local endpoint (via PUSH_PROMISE sent)
    reserved_local,

    /// Stream reserved by remote endpoint (via PUSH_PROMISE received)
    reserved_remote,

    /// Stream is fully open (both directions active)
    open,

    /// Local endpoint has sent END_STREAM
    half_closed_local,

    /// Remote endpoint has sent END_STREAM
    half_closed_remote,

    /// Stream is closed
    closed
};

} // http2
} // beast
} // boost

#endif
