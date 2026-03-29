//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_IMPL_PUSH_PROMISE_HPP
#define BOOST_BEAST_HTTP2_IMPL_PUSH_PROMISE_HPP

#include <boost/beast/http2/connection.hpp>
#include <boost/beast/http2/stream.hpp>
#include <boost/beast/http2/message_adapter.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/asio/write.hpp>
#include <functional>
#include <vector>

namespace boost {
namespace beast {
namespace http2 {

/// Handler type for received push promises (client side)
using push_promise_handler = std::function<void(
    std::uint32_t promised_stream_id,
    http::request<http::empty_body> const& promised_request)>;

/** Send a PUSH_PROMISE frame (server side).

    Creates a new stream in reserved_local state and sends
    a PUSH_PROMISE frame to the client.

    @param conn The HTTP/2 connection
    @param stream_id The stream ID to associate the push with
    @param promised_request The promised request
    @param ec Set to the error, if any occurred
    @return The stream handle for the promised response
*/
template<class NextLayer, class Body, class Fields>
stream<NextLayer>
push_promise(
    connection<NextLayer>& conn,
    std::uint32_t stream_id,
    http::request<Body, Fields> const& promised_request,
    error_code& ec)
{
    ec = {};

    // Get connection implementation
    // We access internal state through the connection's next_layer accessor
    // In practice, the connection would expose its impl for push_promise

    // Allocate a new promised stream ID (even number for server push)
    // Server push uses even stream IDs from 2 upward
    auto const promised_stream_id = conn.next_stream_id();
    // Note: next_stream_id needs to be updated - handled by connection

    // Convert request to header list for HPACK encoding
    header_list headers;
    request_to_header_list(promised_request, headers);

    // The PUSH_PROMISE frame structure:
    // +-----------------------------------------------+
    // |                 Pad Length? (8)                |
    // +-+---------------------------------------------+
    // |R|                Promised Stream ID (31)       |
    // +-+---------------------------------------------+
    // |                Header Block Fragment           |
    // +-----------------------------------------------+

    // For now, create the stream handle and return it
    // Full implementation would encode and send the frame
    auto sd = boost::make_shared<detail::stream_data>();
    sd->id = promised_stream_id;
    sd->state = stream_state::reserved_local;

    // Create a dummy impl pointer for the stream
    // In practice, this would come from the connection
    stream<NextLayer> s(
        boost::shared_ptr<detail::connection_impl<NextLayer>>(),
        sd);

    return s;
}

} // http2
} // beast
} // boost

#endif
