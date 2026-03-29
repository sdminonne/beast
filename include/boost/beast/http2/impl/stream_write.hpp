//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_IMPL_STREAM_WRITE_HPP
#define BOOST_BEAST_HTTP2_IMPL_STREAM_WRITE_HPP

#include <boost/beast/http2/stream.hpp>
#include <boost/beast/http2/message_adapter.hpp>
#include <boost/beast/http2/hpack/encoder.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/asio/write.hpp>
#include <algorithm>
#include <cstring>
#include <vector>

namespace boost {
namespace beast {
namespace http2 {

/** Write a complete HTTP/2 message to a stream (synchronous).

    Encodes headers using HPACK, sends HEADERS frames
    (with CONTINUATION if needed), then sends DATA frames
    with flow control. Sets END_STREAM on the final frame.

    @param s The HTTP/2 stream
    @param msg The message to send
    @param ec Set to the error, if any occurred
*/
template<class NextLayer, bool isRequest, class Body, class Fields>
void
write(
    stream<NextLayer>& s,
    http::message<isRequest, Body, Fields> const& msg,
    error_code& ec)
{
    ec = {};
    auto sp = s.connection_impl();
    if(!sp)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::stream_closed);
        return;
    }

    auto sd = s.data();
    if(!sd)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::stream_closed);
        return;
    }

    // Convert message to header list
    header_list headers;
    if(isRequest)
    {
        request_to_header_list(
            reinterpret_cast<http::request<Body, Fields> const&>(msg),
            headers);
    }
    else
    {
        response_to_header_list(
            reinterpret_cast<http::response<Body, Fields> const&>(msg),
            headers);
    }

    // HPACK encode headers
    std::vector<std::uint8_t> header_block(4096);
    std::size_t header_block_size = 0;

    for(auto const& h : headers)
    {
        // Ensure enough space
        if(header_block_size + h.first.size() + h.second.size() + 64 >
            header_block.size())
        {
            header_block.resize(header_block.size() * 2);
        }

        auto n = sp->hpack_encoder.encode_field(
            header_block.data() + header_block_size,
            header_block.size() - header_block_size,
            h.first, h.second);
        header_block_size += n;
    }

    // Determine if we have a body
    // We serialize the body by accessing msg.body() and converting to bytes
    std::vector<std::uint8_t> body_data;
    {
        auto const& body = msg.body();
        auto const body_begin = reinterpret_cast<
            std::uint8_t const*>(body.data());
        body_data.assign(body_begin, body_begin + body.size());
    }

    bool has_body = !body_data.empty();
    bool end_stream_on_headers = !has_body;

    // Send HEADERS frame(s)
    auto const max_payload = sp->remote_settings.max_frame_size;
    std::size_t header_offset = 0;
    bool first_header_frame = true;

    while(header_offset < header_block_size)
    {
        std::size_t chunk = std::min<std::size_t>(
            header_block_size - header_offset, max_payload);
        bool last_header_frame =
            (header_offset + chunk >= header_block_size);

        frame_header fh;
        fh.length = static_cast<std::uint32_t>(chunk);
        fh.stream_id = s.id();
        fh.flags = 0;

        if(first_header_frame)
        {
            fh.type = frame_type::headers;
            if(last_header_frame)
                fh.flags |= frame_flag::end_headers;
            if(end_stream_on_headers)
                fh.flags |= frame_flag::end_stream;
            first_header_frame = false;
        }
        else
        {
            fh.type = frame_type::continuation;
            if(last_header_frame)
                fh.flags |= frame_flag::end_headers;
        }

        std::uint8_t frame_buf[frame_header_size];
        serialize_frame_header(frame_buf, fh);

        net::write(sp->stream,
            net::const_buffer(frame_buf, frame_header_size), ec);
        if(ec)
            return;

        net::write(sp->stream,
            net::const_buffer(
                header_block.data() + header_offset, chunk), ec);
        if(ec)
            return;

        header_offset += chunk;
    }

    if(end_stream_on_headers)
    {
        sd->end_stream_sent = true;
        sd->state = (sd->end_stream_received)
            ? stream_state::closed
            : stream_state::half_closed_local;
        return;
    }

    // Send DATA frames with flow control
    std::size_t data_offset = 0;
    while(data_offset < body_data.size())
    {
        // Determine sendable bytes (respect both connection and stream windows)
        std::size_t max_send = std::min<std::size_t>(
            max_payload, body_data.size() - data_offset);
        if(sp->send_window > 0)
            max_send = std::min<std::size_t>(max_send,
                static_cast<std::size_t>(sp->send_window));
        if(sd->send_window > 0)
            max_send = std::min<std::size_t>(max_send,
                static_cast<std::size_t>(sd->send_window));

        if(max_send == 0)
        {
            // Flow control blocked - in a real implementation
            // we'd wait for WINDOW_UPDATE
            BOOST_BEAST_ASSIGN_EC(ec, error::flow_control_error);
            return;
        }

        bool last_data_frame =
            (data_offset + max_send >= body_data.size());

        frame_header fh;
        fh.length = static_cast<std::uint32_t>(max_send);
        fh.type = frame_type::data;
        fh.flags = last_data_frame ? frame_flag::end_stream
                                   : static_cast<std::uint8_t>(0);
        fh.stream_id = s.id();

        std::uint8_t frame_buf[frame_header_size];
        serialize_frame_header(frame_buf, fh);

        net::write(sp->stream,
            net::const_buffer(frame_buf, frame_header_size), ec);
        if(ec)
            return;

        net::write(sp->stream,
            net::const_buffer(
                body_data.data() + data_offset, max_send), ec);
        if(ec)
            return;

        // Update flow control windows
        sp->send_window -= static_cast<std::int32_t>(max_send);
        sd->send_window -= static_cast<std::int32_t>(max_send);

        data_offset += max_send;
    }

    sd->end_stream_sent = true;
    sd->state = (sd->end_stream_received)
        ? stream_state::closed
        : stream_state::half_closed_local;
}

/** Write a complete HTTP/2 message to a stream (asynchronous).

    @param s The HTTP/2 stream
    @param msg The message to send
    @param handler The completion handler
*/
template<
    class NextLayer,
    bool isRequest,
    class Body,
    class Fields,
    class WriteHandler>
auto
async_write(
    stream<NextLayer>& s,
    http::message<isRequest, Body, Fields> const& msg,
    WriteHandler&& handler)
{
    return net::async_initiate<WriteHandler, void(error_code)>(
        [&s, &msg](auto handler)
        {
            error_code ec;
            write(s, msg, ec);
            auto ex = net::get_associated_executor(
                handler, s.get_executor());
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
