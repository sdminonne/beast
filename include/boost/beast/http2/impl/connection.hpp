//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_IMPL_CONNECTION_HPP
#define BOOST_BEAST_HTTP2_IMPL_CONNECTION_HPP

#include <boost/beast/http2/connection.hpp>
#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>
#include <boost/make_shared.hpp>
#include <cstring>

namespace boost {
namespace beast {
namespace http2 {

// Client connection preface magic (RFC 9113 Section 3.4)
namespace detail {
constexpr char client_preface_magic[] =
    "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
constexpr std::size_t client_preface_magic_size = 24;
} // detail

template<class NextLayer>
template<class... Args>
connection<NextLayer>::connection(Args&&... args)
    : impl_(boost::make_shared<
        detail::connection_impl<NextLayer>>(
            std::forward<Args>(args)...))
{
}

template<class NextLayer>
auto
connection<NextLayer>::get_executor() noexcept ->
    executor_type
{
    return impl_->stream.get_executor();
}

template<class NextLayer>
auto
connection<NextLayer>::next_layer() noexcept ->
    next_layer_type&
{
    return impl_->stream;
}

template<class NextLayer>
auto
connection<NextLayer>::next_layer() const noexcept ->
    next_layer_type const&
{
    return impl_->stream;
}

template<class NextLayer>
void
connection<NextLayer>::set_option(options const& opt)
{
    impl_->conn_opt = opt;
    impl_->local_settings = opt.initial_settings;
}

template<class NextLayer>
void
connection<NextLayer>::set_option(timeout const& t)
{
    impl_->timeout_opt = t;
}

template<class NextLayer>
settings const&
connection<NextLayer>::local_settings() const noexcept
{
    return impl_->local_settings;
}

template<class NextLayer>
settings const&
connection<NextLayer>::remote_settings() const noexcept
{
    return impl_->remote_settings;
}

template<class NextLayer>
std::uint32_t
connection<NextLayer>::next_stream_id() const noexcept
{
    return impl_->next_stream_id;
}

template<class NextLayer>
bool
connection<NextLayer>::is_open() const noexcept
{
    return impl_->preface_sent &&
           impl_->preface_received &&
           !impl_->goaway_sent &&
           !impl_->goaway_received;
}

template<class NextLayer>
void
connection<NextLayer>::send_frame(
    frame_header const& fh,
    void const* payload,
    error_code& ec)
{
    std::uint8_t header_buf[frame_header_size];
    serialize_frame_header(header_buf, fh);

    // Write header
    net::write(impl_->stream,
        net::const_buffer(header_buf, frame_header_size), ec);
    if(ec)
        return;

    // Write payload if any
    if(fh.length > 0 && payload)
    {
        net::write(impl_->stream,
            net::const_buffer(payload, fh.length), ec);
    }
}

template<class NextLayer>
void
connection<NextLayer>::send_settings(error_code& ec)
{
    std::uint8_t payload[36]; // max 6 settings * 6 bytes
    auto n = serialize_settings(payload, impl_->local_settings);

    frame_header fh;
    fh.length = static_cast<std::uint32_t>(n);
    fh.type = frame_type::settings;
    fh.flags = 0;
    fh.stream_id = 0;

    send_frame(fh, payload, ec);
}

template<class NextLayer>
void
connection<NextLayer>::send_settings_ack(error_code& ec)
{
    frame_header fh;
    fh.length = 0;
    fh.type = frame_type::settings;
    fh.flags = frame_flag::ack;
    fh.stream_id = 0;

    send_frame(fh, nullptr, ec);
}

template<class NextLayer>
void
connection<NextLayer>::read_settings(error_code& ec)
{
    // Read frame header
    auto& buf = impl_->rd_buf;
    while(buf.size() < frame_header_size)
    {
        auto bytes = impl_->stream.read_some(
            buf.prepare(frame_header_size - buf.size()), ec);
        if(ec)
            return;
        buf.commit(bytes);
    }

    // Parse frame header
    frame_header fh;
    auto const* data = static_cast<std::uint8_t const*>(buf.data().data());
    parse_frame_header(fh, data, ec);
    if(ec)
        return;
    buf.consume(frame_header_size);

    if(fh.type != frame_type::settings || (fh.flags & frame_flag::ack))
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::protocol_error);
        return;
    }

    if(fh.stream_id != 0)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::protocol_error);
        return;
    }

    // Read payload
    while(buf.size() < fh.length)
    {
        auto bytes = impl_->stream.read_some(
            buf.prepare(fh.length - buf.size()), ec);
        if(ec)
            return;
        buf.commit(bytes);
    }

    auto const* payload = static_cast<std::uint8_t const*>(buf.data().data());
    parse_settings(impl_->remote_settings, payload, fh.length, ec);
    buf.consume(fh.length);

    if(ec)
        return;

    // Update HPACK decoder table size from remote settings
    impl_->hpack_decoder.set_max_table_size(
        impl_->remote_settings.header_table_size);
}

template<class NextLayer>
void
connection<NextLayer>::do_handshake(error_code& ec)
{
    // Client handshake:
    // 1. Send connection preface magic
    net::write(impl_->stream, net::const_buffer(
        detail::client_preface_magic,
        detail::client_preface_magic_size), ec);
    if(ec)
        return;

    // 2. Send SETTINGS
    send_settings(ec);
    if(ec)
        return;

    impl_->preface_sent = true;
    impl_->is_server = false;
    impl_->next_stream_id = 1; // Client uses odd stream IDs

    // 3. Read server SETTINGS
    read_settings(ec);
    if(ec)
        return;

    // 4. Send SETTINGS ACK
    send_settings_ack(ec);
    if(ec)
        return;

    // 5. Read SETTINGS ACK from server
    // (simplified: read next frame, expect SETTINGS ACK)
    auto& buf = impl_->rd_buf;
    while(buf.size() < frame_header_size)
    {
        auto bytes = impl_->stream.read_some(
            buf.prepare(frame_header_size - buf.size()), ec);
        if(ec)
            return;
        buf.commit(bytes);
    }

    frame_header fh;
    auto const* fh_data = static_cast<std::uint8_t const*>(buf.data().data());
    parse_frame_header(fh, fh_data, ec);
    if(ec)
        return;
    buf.consume(frame_header_size);

    if(fh.type != frame_type::settings || !(fh.flags & frame_flag::ack))
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::protocol_error);
        return;
    }

    // Consume any payload (SETTINGS ACK should have 0 length)
    if(fh.length > 0)
    {
        while(buf.size() < fh.length)
        {
            auto bytes = impl_->stream.read_some(
                buf.prepare(fh.length - buf.size()), ec);
            if(ec)
                return;
            buf.commit(bytes);
        }
        buf.consume(fh.length);
    }

    impl_->preface_received = true;
}

template<class NextLayer>
void
connection<NextLayer>::do_accept(error_code& ec)
{
    impl_->is_server = true;
    impl_->next_stream_id = 2; // Server uses even stream IDs

    // Server handshake:
    // 1. Read client preface magic (24 bytes)
    auto& buf = impl_->rd_buf;
    while(buf.size() < detail::client_preface_magic_size)
    {
        auto bytes = impl_->stream.read_some(
            buf.prepare(detail::client_preface_magic_size - buf.size()), ec);
        if(ec)
            return;
        buf.commit(bytes);
    }

    auto const* preface_data = static_cast<char const*>(buf.data().data());
    if(std::memcmp(preface_data, detail::client_preface_magic,
                   detail::client_preface_magic_size) != 0)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::bad_preface);
        return;
    }
    buf.consume(detail::client_preface_magic_size);

    // 2. Read client SETTINGS
    read_settings(ec);
    if(ec)
        return;

    // 3. Send server SETTINGS
    send_settings(ec);
    if(ec)
        return;

    // 4. Send SETTINGS ACK for client settings
    send_settings_ack(ec);
    if(ec)
        return;

    impl_->preface_sent = true;

    // 5. Read SETTINGS ACK from client
    while(buf.size() < frame_header_size)
    {
        auto bytes = impl_->stream.read_some(
            buf.prepare(frame_header_size - buf.size()), ec);
        if(ec)
            return;
        buf.commit(bytes);
    }

    frame_header fh;
    auto const* fh_data = static_cast<std::uint8_t const*>(buf.data().data());
    parse_frame_header(fh, fh_data, ec);
    if(ec)
        return;
    buf.consume(frame_header_size);

    if(fh.type != frame_type::settings || !(fh.flags & frame_flag::ack))
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::protocol_error);
        return;
    }

    if(fh.length > 0)
    {
        while(buf.size() < fh.length)
        {
            auto bytes = impl_->stream.read_some(
                buf.prepare(fh.length - buf.size()), ec);
            if(ec)
                return;
            buf.commit(bytes);
        }
        buf.consume(fh.length);
    }

    impl_->preface_received = true;
}

template<class NextLayer>
void
connection<NextLayer>::handshake(error_code& ec)
{
    do_handshake(ec);
}

template<class NextLayer>
void
connection<NextLayer>::accept(error_code& ec)
{
    do_accept(ec);
}

template<class NextLayer>
void
connection<NextLayer>::send_ping(
    net::const_buffer payload,
    bool ack,
    error_code& ec)
{
    if(payload.size() != 8)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::frame_size_error);
        return;
    }

    frame_header fh;
    fh.length = 8;
    fh.type = frame_type::ping;
    fh.flags = ack ? frame_flag::ack : static_cast<std::uint8_t>(0);
    fh.stream_id = 0;

    send_frame(fh, payload.data(), ec);
}

template<class NextLayer>
void
connection<NextLayer>::ping(
    net::const_buffer payload,
    error_code& ec)
{
    send_ping(payload, false, ec);
}

template<class NextLayer>
void
connection<NextLayer>::send_goaway(
    std::uint32_t last_stream_id,
    std::uint32_t code,
    error_code& ec)
{
    std::uint8_t payload[8];
    // Last-Stream-ID (31 bits)
    payload[0] = static_cast<std::uint8_t>((last_stream_id >> 24) & 0x7f);
    payload[1] = static_cast<std::uint8_t>((last_stream_id >> 16) & 0xff);
    payload[2] = static_cast<std::uint8_t>((last_stream_id >> 8) & 0xff);
    payload[3] = static_cast<std::uint8_t>(last_stream_id & 0xff);
    // Error Code
    payload[4] = static_cast<std::uint8_t>((code >> 24) & 0xff);
    payload[5] = static_cast<std::uint8_t>((code >> 16) & 0xff);
    payload[6] = static_cast<std::uint8_t>((code >> 8) & 0xff);
    payload[7] = static_cast<std::uint8_t>(code & 0xff);

    frame_header fh;
    fh.length = 8;
    fh.type = frame_type::goaway;
    fh.flags = 0;
    fh.stream_id = 0;

    send_frame(fh, payload, ec);
    if(! ec)
    {
        impl_->goaway_sent = true;
        impl_->goaway_last_stream_id = last_stream_id;
    }
}

template<class NextLayer>
void
connection<NextLayer>::goaway(
    std::uint32_t last_stream_id,
    std::uint32_t code,
    error_code& ec)
{
    send_goaway(last_stream_id, code, ec);
}

template<class NextLayer>
void
connection<NextLayer>::send_window_update(
    std::uint32_t stream_id,
    std::uint32_t increment,
    error_code& ec)
{
    std::uint8_t payload[4];
    payload[0] = static_cast<std::uint8_t>((increment >> 24) & 0x7f);
    payload[1] = static_cast<std::uint8_t>((increment >> 16) & 0xff);
    payload[2] = static_cast<std::uint8_t>((increment >> 8) & 0xff);
    payload[3] = static_cast<std::uint8_t>(increment & 0xff);

    frame_header fh;
    fh.length = 4;
    fh.type = frame_type::window_update;
    fh.flags = 0;
    fh.stream_id = stream_id;

    send_frame(fh, payload, ec);
}

template<class NextLayer>
void
connection<NextLayer>::dispatch_frame(
    frame_header const& fh,
    std::uint8_t const* payload,
    error_code& ec)
{
    switch(fh.type)
    {
    case frame_type::settings:
    {
        if(fh.flags & frame_flag::ack)
        {
            // SETTINGS ACK - nothing to do
            break;
        }
        if(fh.stream_id != 0)
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::protocol_error);
            return;
        }
        // Parse and apply new settings
        settings new_settings = impl_->remote_settings;
        parse_settings(new_settings, payload, fh.length, ec);
        if(ec)
            return;

        // Check if initial_window_size changed
        std::int32_t delta = static_cast<std::int32_t>(
            new_settings.initial_window_size) -
            static_cast<std::int32_t>(
                impl_->remote_settings.initial_window_size);
        impl_->remote_settings = new_settings;

        // Update stream send windows
        if(delta != 0)
        {
            for(auto& pair : impl_->streams)
            {
                auto& sd = pair.second;
                if(sd)
                {
                    // Adjust will be handled by stream_data
                    (void)delta;
                }
            }
        }

        // Update HPACK decoder table size
        impl_->hpack_decoder.set_max_table_size(
            impl_->remote_settings.header_table_size);

        // Send ACK
        send_settings_ack(ec);
        break;
    }

    case frame_type::ping:
    {
        if(fh.stream_id != 0)
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::protocol_error);
            return;
        }
        if(fh.length != 8)
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::frame_size_error);
            return;
        }
        if(fh.flags & frame_flag::ack)
        {
            // PING ACK - notify waiter if any
            break;
        }
        // Send PING ACK
        send_ping(net::const_buffer(payload, 8), true, ec);
        break;
    }

    case frame_type::goaway:
    {
        if(fh.stream_id != 0)
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::protocol_error);
            return;
        }
        if(fh.length < 8)
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::frame_size_error);
            return;
        }
        impl_->goaway_received = true;
        impl_->goaway_last_stream_id =
            (static_cast<std::uint32_t>(payload[0] & 0x7f) << 24) |
            (static_cast<std::uint32_t>(payload[1]) << 16) |
            (static_cast<std::uint32_t>(payload[2]) << 8) |
            static_cast<std::uint32_t>(payload[3]);
        impl_->goaway_error_code =
            (static_cast<std::uint32_t>(payload[4]) << 24) |
            (static_cast<std::uint32_t>(payload[5]) << 16) |
            (static_cast<std::uint32_t>(payload[6]) << 8) |
            static_cast<std::uint32_t>(payload[7]);
        break;
    }

    case frame_type::window_update:
    {
        if(fh.length != 4)
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::frame_size_error);
            return;
        }
        std::uint32_t increment =
            (static_cast<std::uint32_t>(payload[0] & 0x7f) << 24) |
            (static_cast<std::uint32_t>(payload[1]) << 16) |
            (static_cast<std::uint32_t>(payload[2]) << 8) |
            static_cast<std::uint32_t>(payload[3]);

        if(increment == 0)
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::protocol_error);
            return;
        }

        if(fh.stream_id == 0)
        {
            // Connection-level window update
            std::int64_t new_window =
                static_cast<std::int64_t>(impl_->send_window) + increment;
            if(new_window > max_window_size)
            {
                BOOST_BEAST_ASSIGN_EC(ec, error::flow_control_error);
                return;
            }
            impl_->send_window = static_cast<std::int32_t>(new_window);
        }
        else
        {
            // Stream-level window update - dispatch to stream
            auto it = impl_->streams.find(fh.stream_id);
            if(it != impl_->streams.end())
            {
                // Stream-level flow control handled in stream layer
            }
        }
        break;
    }

    case frame_type::rst_stream:
    {
        if(fh.stream_id == 0)
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::protocol_error);
            return;
        }
        if(fh.length != 4)
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::frame_size_error);
            return;
        }
        // Mark stream as closed
        auto it = impl_->streams.find(fh.stream_id);
        if(it != impl_->streams.end())
        {
            impl_->streams.erase(it);
        }
        break;
    }

    case frame_type::data:
    case frame_type::headers:
    case frame_type::continuation:
    case frame_type::push_promise:
    case frame_type::priority:
        // These are dispatched to stream handlers
        // (handled in the stream read/write layer)
        break;
    }
}

// Async operations (placeholder implementations using sync)
template<class NextLayer>
template<BOOST_BEAST_ASYNC_TPARAM1 HandshakeHandler>
BOOST_BEAST_ASYNC_RESULT1(HandshakeHandler)
connection<NextLayer>::async_handshake(HandshakeHandler&& handler)
{
    return net::async_initiate<HandshakeHandler, void(error_code)>(
        [this](auto handler)
        {
            error_code ec;
            this->handshake(ec);
            auto ex = net::get_associated_executor(
                handler, this->get_executor());
            net::post(ex, [handler = std::move(handler), ec]() mutable
            {
                handler(ec);
            });
        },
        handler);
}

template<class NextLayer>
template<BOOST_BEAST_ASYNC_TPARAM1 AcceptHandler>
BOOST_BEAST_ASYNC_RESULT1(AcceptHandler)
connection<NextLayer>::async_accept(AcceptHandler&& handler)
{
    return net::async_initiate<AcceptHandler, void(error_code)>(
        [this](auto handler)
        {
            error_code ec;
            this->accept(ec);
            auto ex = net::get_associated_executor(
                handler, this->get_executor());
            net::post(ex, [handler = std::move(handler), ec]() mutable
            {
                handler(ec);
            });
        },
        handler);
}

template<class NextLayer>
template<BOOST_BEAST_ASYNC_TPARAM1 PingHandler>
BOOST_BEAST_ASYNC_RESULT1(PingHandler)
connection<NextLayer>::async_ping(
    net::const_buffer payload,
    PingHandler&& handler)
{
    return net::async_initiate<PingHandler, void(error_code)>(
        [this, payload](auto handler)
        {
            error_code ec;
            this->ping(payload, ec);
            auto ex = net::get_associated_executor(
                handler, this->get_executor());
            net::post(ex, [handler = std::move(handler), ec]() mutable
            {
                handler(ec);
            });
        },
        handler);
}

template<class NextLayer>
template<BOOST_BEAST_ASYNC_TPARAM1 GoawayHandler>
BOOST_BEAST_ASYNC_RESULT1(GoawayHandler)
connection<NextLayer>::async_goaway(
    std::uint32_t last_stream_id,
    std::uint32_t code,
    GoawayHandler&& handler)
{
    return net::async_initiate<GoawayHandler, void(error_code)>(
        [this, last_stream_id, code](auto handler)
        {
            error_code ec;
            this->goaway(last_stream_id, code, ec);
            auto ex = net::get_associated_executor(
                handler, this->get_executor());
            net::post(ex, [handler = std::move(handler), ec]() mutable
            {
                handler(ec);
            });
        },
        handler);
}

template<class NextLayer>
template<BOOST_BEAST_ASYNC_TPARAM1 RunHandler>
BOOST_BEAST_ASYNC_RESULT1(RunHandler)
connection<NextLayer>::async_run(RunHandler&& handler)
{
    return net::async_initiate<RunHandler, void(error_code)>(
        [this](auto handler)
        {
            error_code ec;
            auto& buf = impl_->rd_buf;

            while(! ec)
            {
                // Read frame header
                while(buf.size() < frame_header_size)
                {
                    auto bytes = impl_->stream.read_some(
                        buf.prepare(4096), ec);
                    if(ec)
                        break;
                    buf.commit(bytes);
                }
                if(ec)
                    break;

                frame_header fh;
                auto const* hdr_data = static_cast<
                    std::uint8_t const*>(buf.data().data());
                parse_frame_header(fh, hdr_data, ec);
                if(ec)
                    break;
                buf.consume(frame_header_size);

                // Read payload
                while(buf.size() < fh.length)
                {
                    auto bytes = impl_->stream.read_some(
                        buf.prepare(fh.length - buf.size()), ec);
                    if(ec)
                        break;
                    buf.commit(bytes);
                }
                if(ec)
                    break;

                auto const* payload = static_cast<
                    std::uint8_t const*>(buf.data().data());
                dispatch_frame(fh, payload, ec);
                buf.consume(fh.length);
            }

            auto ex = net::get_associated_executor(
                handler, this->get_executor());
            net::post(ex, [handler = std::move(handler), ec]() mutable
            {
                handler(ec);
            });
        },
        handler);
}

} // http2
} // beast
} // boost

#endif
