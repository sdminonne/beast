//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_STREAM_HPP
#define BOOST_BEAST_HTTP2_STREAM_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/http2/detail/connection_impl.hpp>
#include <boost/beast/http2/detail/stream_impl.hpp>
#include <boost/beast/http2/stream_state.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/write.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <cstdint>

namespace boost {
namespace beast {
namespace http2 {

/** An HTTP/2 stream handle.

    Represents a single HTTP/2 stream within a connection.
    Each stream has its own state, flow control windows,
    and supports independent read/write operations.

    Streams hold a weak_ptr to the connection implementation
    to detect connection teardown.

    @tparam NextLayer The type of the connection's next layer
*/
template<class NextLayer>
class stream
{
    boost::weak_ptr<detail::connection_impl<NextLayer>> wp_;
    boost::shared_ptr<detail::stream_data> sd_;
    std::uint32_t stream_id_;

public:
    /// The type of the executor associated with the object
    using executor_type =
        beast::executor_type<typename
            std::remove_reference<NextLayer>::type>;

    /** Constructor.

        Creates a stream handle associated with a connection.

        @param impl Shared pointer to the connection implementation
        @param sd Shared pointer to the stream data
    */
    stream(
        boost::shared_ptr<detail::connection_impl<NextLayer>> const& impl,
        boost::shared_ptr<detail::stream_data> const& sd)
        : wp_(impl)
        , sd_(sd)
        , stream_id_(sd ? sd->id : 0)
    {
    }

    /// Returns the executor associated with the object
    executor_type
    get_executor() noexcept
    {
        auto sp = wp_.lock();
        return sp->stream.get_executor();
    }

    /// Returns the stream identifier
    std::uint32_t
    id() const noexcept
    {
        return stream_id_;
    }

    /// Returns the current stream state
    stream_state
    state() const
    {
        if(sd_)
            return sd_->state;
        return stream_state::closed;
    }

    /// Returns true if the stream is open for sending or receiving
    bool
    is_open() const
    {
        if(!sd_)
            return false;
        auto s = sd_->state;
        return s == stream_state::open ||
               s == stream_state::half_closed_local ||
               s == stream_state::half_closed_remote;
    }

    /** Send a RST_STREAM frame (synchronous).

        @param error_code The error code to send
        @param ec Set to the error, if any occurred
    */
    void
    reset(std::uint32_t err_code, error_code& ec)
    {
        auto sp = wp_.lock();
        if(!sp)
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::stream_closed);
            return;
        }

        std::uint8_t payload[4];
        payload[0] = static_cast<std::uint8_t>((err_code >> 24) & 0xff);
        payload[1] = static_cast<std::uint8_t>((err_code >> 16) & 0xff);
        payload[2] = static_cast<std::uint8_t>((err_code >> 8) & 0xff);
        payload[3] = static_cast<std::uint8_t>(err_code & 0xff);

        frame_header fh;
        fh.length = 4;
        fh.type = frame_type::rst_stream;
        fh.flags = 0;
        fh.stream_id = stream_id_;

        std::uint8_t header_buf[frame_header_size];
        serialize_frame_header(header_buf, fh);

        net::write(sp->stream,
            net::const_buffer(header_buf, frame_header_size), ec);
        if(ec)
            return;

        net::write(sp->stream,
            net::const_buffer(payload, 4), ec);
        if(ec)
            return;

        if(sd_)
        {
            sd_->rst_sent = true;
            sd_->state = stream_state::closed;
        }
    }

    /** Send a RST_STREAM frame (asynchronous).

        @param err_code The error code to send
        @param handler The completion handler
    */
    template<
        BOOST_BEAST_ASYNC_TPARAM1 ResetHandler =
            net::default_completion_token_t<executor_type>>
    BOOST_BEAST_ASYNC_RESULT1(ResetHandler)
    async_reset(
        std::uint32_t err_code,
        ResetHandler&& handler =
            net::default_completion_token_t<executor_type>{})
    {
        return net::async_initiate<ResetHandler, void(beast::error_code)>(
            [this, err_code](auto handler)
            {
                beast::error_code ec;
                this->reset(err_code, ec);
                auto ex = net::get_associated_executor(
                    handler, this->get_executor());
                net::post(ex, [handler = std::move(handler), ec]() mutable
                {
                    handler(ec);
                });
            },
            handler);
    }

    /// Access the stream data (for internal use)
    boost::shared_ptr<detail::stream_data> const&
    data() const noexcept
    {
        return sd_;
    }

    /// Access the connection implementation (for internal use)
    boost::shared_ptr<detail::connection_impl<NextLayer>>
    connection_impl() const
    {
        return wp_.lock();
    }
};

} // http2
} // beast
} // boost

#include <boost/beast/http2/impl/stream.hpp>

#endif
