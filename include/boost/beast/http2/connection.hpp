//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_CONNECTION_HPP
#define BOOST_BEAST_HTTP2_CONNECTION_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/http2/connection_base.hpp>
#include <boost/beast/http2/connection_fwd.hpp>
#include <boost/beast/http2/detail/connection_impl.hpp>
#include <boost/beast/http2/error.hpp>
#include <boost/beast/http2/frame.hpp>
#include <boost/beast/http2/settings.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/shared_ptr.hpp>
#include <cstdint>
#include <type_traits>

namespace boost {
namespace beast {
namespace http2 {

/** An HTTP/2 connection.

    This class represents an HTTP/2 connection over a stream.
    It manages the connection-level state including HPACK codec,
    settings, flow control, and stream management.

    The connection owns the underlying transport and provides
    operations for the HTTP/2 connection preface exchange,
    PING, GOAWAY, and the persistent read pump.

    @tparam NextLayer The type of the next layer in the stack.
*/
template<class NextLayer>
class connection
    : private connection_base
{
    boost::shared_ptr<detail::connection_impl<NextLayer>> impl_;

public:
    /// The type of the next layer
    using next_layer_type = typename
        std::remove_reference<NextLayer>::type;

    /// The type of the executor associated with the object
    using executor_type =
        beast::executor_type<next_layer_type>;

    /** Constructor.

        Constructs a connection, forwarding arguments to the
        next layer constructor.

        @param args Arguments forwarded to the next layer
    */
    template<class... Args>
    explicit
    connection(Args&&... args);

    /// Returns the executor associated with the object
    executor_type
    get_executor() noexcept;

    /// Returns a reference to the next layer
    next_layer_type&
    next_layer() noexcept;

    /// Returns a const reference to the next layer
    next_layer_type const&
    next_layer() const noexcept;

    /** Set connection options.

        Must be called before handshake/accept.

        @param opt The connection options
    */
    void
    set_option(options const& opt);

    /** Set timeout options.

        @param t The timeout configuration
    */
    void
    set_option(timeout const& t);

    /// Returns the local settings
    settings const&
    local_settings() const noexcept;

    /// Returns the remote (peer) settings
    settings const&
    remote_settings() const noexcept;

    /// Returns the next stream ID to be used
    std::uint32_t
    next_stream_id() const noexcept;

    /// Returns true if the connection is open
    bool
    is_open() const noexcept;

    /** Perform the client-side handshake (synchronous).

        Sends the connection preface (magic octets + SETTINGS),
        reads the server's SETTINGS, and exchanges ACKs.

        @param ec Set to the error, if any occurred
    */
    void
    handshake(error_code& ec);

    /** Perform the client-side handshake (asynchronous).

        @param handler The completion handler
    */
    template<
        BOOST_BEAST_ASYNC_TPARAM1 HandshakeHandler =
            net::default_completion_token_t<executor_type>>
    BOOST_BEAST_ASYNC_RESULT1(HandshakeHandler)
    async_handshake(
        HandshakeHandler&& handler =
            net::default_completion_token_t<executor_type>{});

    /** Accept a client connection (synchronous).

        Reads the client preface (magic octets + SETTINGS),
        sends SETTINGS, and exchanges ACKs.

        @param ec Set to the error, if any occurred
    */
    void
    accept(error_code& ec);

    /** Accept a client connection (asynchronous).

        @param handler The completion handler
    */
    template<
        BOOST_BEAST_ASYNC_TPARAM1 AcceptHandler =
            net::default_completion_token_t<executor_type>>
    BOOST_BEAST_ASYNC_RESULT1(AcceptHandler)
    async_accept(
        AcceptHandler&& handler =
            net::default_completion_token_t<executor_type>{});

    /** Send a PING frame (synchronous).

        @param payload 8-byte PING payload
        @param ec Set to the error, if any occurred
    */
    void
    ping(net::const_buffer payload, error_code& ec);

    /** Send a PING frame (asynchronous).

        @param payload 8-byte PING payload
        @param handler The completion handler
    */
    template<
        BOOST_BEAST_ASYNC_TPARAM1 PingHandler =
            net::default_completion_token_t<executor_type>>
    BOOST_BEAST_ASYNC_RESULT1(PingHandler)
    async_ping(
        net::const_buffer payload,
        PingHandler&& handler =
            net::default_completion_token_t<executor_type>{});

    /** Send a GOAWAY frame (synchronous).

        @param last_stream_id The last stream ID the sender will process
        @param code The error code (0 for graceful shutdown)
        @param ec Set to the error, if any occurred
    */
    void
    goaway(
        std::uint32_t last_stream_id,
        std::uint32_t code,
        error_code& ec);

    /** Send a GOAWAY frame (asynchronous).

        @param last_stream_id The last stream ID
        @param code The error code
        @param handler The completion handler
    */
    template<
        BOOST_BEAST_ASYNC_TPARAM1 GoawayHandler =
            net::default_completion_token_t<executor_type>>
    BOOST_BEAST_ASYNC_RESULT1(GoawayHandler)
    async_goaway(
        std::uint32_t last_stream_id,
        std::uint32_t code,
        GoawayHandler&& handler =
            net::default_completion_token_t<executor_type>{});

    /** Run the connection read pump (asynchronous).

        Continuously reads and dispatches frames from the
        underlying transport. Control frames (SETTINGS, PING,
        GOAWAY, WINDOW_UPDATE) are handled immediately. Data
        and header frames are dispatched to stream handlers.

        This operation runs until the connection is closed or
        an error occurs.

        @param handler The completion handler
    */
    template<
        BOOST_BEAST_ASYNC_TPARAM1 RunHandler =
            net::default_completion_token_t<executor_type>>
    BOOST_BEAST_ASYNC_RESULT1(RunHandler)
    async_run(
        RunHandler&& handler =
            net::default_completion_token_t<executor_type>{});

private:
    void
    do_handshake(error_code& ec);

    void
    do_accept(error_code& ec);

    void
    send_settings(error_code& ec);

    void
    send_settings_ack(error_code& ec);

    void
    read_settings(error_code& ec);

    void
    send_frame(
        frame_header const& fh,
        void const* payload,
        error_code& ec);

    void
    send_ping(
        net::const_buffer payload,
        bool ack,
        error_code& ec);

    void
    send_goaway(
        std::uint32_t last_stream_id,
        std::uint32_t code,
        error_code& ec);

    void
    send_window_update(
        std::uint32_t stream_id,
        std::uint32_t increment,
        error_code& ec);

    void
    dispatch_frame(
        frame_header const& fh,
        std::uint8_t const* payload,
        error_code& ec);
};

} // http2
} // beast
} // boost

#include <boost/beast/http2/impl/connection.hpp>

#endif
