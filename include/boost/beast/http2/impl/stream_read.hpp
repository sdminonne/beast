//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_IMPL_STREAM_READ_HPP
#define BOOST_BEAST_HTTP2_IMPL_STREAM_READ_HPP

#include <boost/beast/http2/stream.hpp>
#include <boost/beast/http2/message_adapter.hpp>
#include <boost/beast/http2/hpack/decoder.hpp>
#include <boost/beast/http/message.hpp>
#include <vector>

namespace boost {
namespace beast {
namespace http2 {

/** Read a complete HTTP/2 message from a stream (synchronous).

    Reads HEADERS and DATA frames from the stream until
    END_STREAM is received, then assembles them into an
    HTTP message.

    @param s The HTTP/2 stream
    @param msg The message to populate
    @param ec Set to the error, if any occurred
*/
template<class NextLayer, bool isRequest, class Body, class Fields>
void
read(
    stream<NextLayer>& s,
    http::message<isRequest, Body, Fields>& msg,
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

    // Read frames until we have headers + end_stream
    auto& buf = sp->rd_buf;
    header_list headers;
    bool got_headers = false;

    while(!sd->end_stream_received)
    {
        // Read frame header
        while(buf.size() < frame_header_size)
        {
            auto bytes = sp->stream.read_some(
                buf.prepare(4096), ec);
            if(ec)
                return;
            buf.commit(bytes);
        }

        frame_header fh;
        auto const* hdr_data = static_cast<
            std::uint8_t const*>(buf.data().data());
        parse_frame_header(fh, hdr_data, ec);
        if(ec)
            return;
        buf.consume(frame_header_size);

        // Read payload
        while(buf.size() < fh.length)
        {
            auto bytes = sp->stream.read_some(
                buf.prepare(fh.length - buf.size()), ec);
            if(ec)
                return;
            buf.commit(bytes);
        }

        auto const* payload = static_cast<
            std::uint8_t const*>(buf.data().data());

        if(fh.type == frame_type::headers)
        {
            // Decode HPACK header block
            struct collector : hpack::decode_handler
            {
                header_list& headers_;
                explicit collector(header_list& h) : headers_(h) {}
                void on_header(string_view name, string_view value,
                    bool, error_code&) override
                {
                    headers_.emplace_back(
                        std::string(name.data(), name.size()),
                        std::string(value.data(), value.size()));
                }
            } handler(headers);

            sp->hpack_decoder.decode(payload, fh.length, handler, ec);
            if(ec)
                return;

            got_headers = true;
            sd->headers_received = true;

            if(fh.flags & frame_flag::end_stream)
            {
                sd->end_stream_received = true;
            }
        }
        else if(fh.type == frame_type::data)
        {
            // Append data to stream buffer
            auto dest = sd->rd_buf.prepare(fh.length);
            std::memcpy(dest.data(), payload, fh.length);
            sd->rd_buf.commit(fh.length);

            // Update flow control
            sd->recv_window -= static_cast<std::int32_t>(fh.length);
            sp->recv_window -= static_cast<std::int32_t>(fh.length);

            if(fh.flags & frame_flag::end_stream)
            {
                sd->end_stream_received = true;
            }
        }

        buf.consume(fh.length);
    }

    if(!got_headers)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::bad_header_block);
        return;
    }

    // Convert header list to message
    if(isRequest)
    {
        header_list_to_request(headers,
            reinterpret_cast<http::request<Body, Fields>&>(msg), ec);
    }
    else
    {
        header_list_to_response(headers,
            reinterpret_cast<http::response<Body, Fields>&>(msg), ec);
    }

    if(ec)
        return;

    // Copy body data from stream buffer to message body
    // (only for non-empty bodies)
    if(sd->rd_buf.size() > 0)
    {
        auto& body = msg.body();
        auto const* body_data = static_cast<char const*>(
            sd->rd_buf.data().data());
        body.assign(body_data, body_data + sd->rd_buf.size());
        sd->rd_buf.consume(sd->rd_buf.size());
    }
}

/** Read a complete HTTP/2 message from a stream (asynchronous).

    @param s The HTTP/2 stream
    @param msg The message to populate
    @param handler The completion handler
*/
template<
    class NextLayer,
    bool isRequest,
    class Body,
    class Fields,
    class ReadHandler>
auto
async_read(
    stream<NextLayer>& s,
    http::message<isRequest, Body, Fields>& msg,
    ReadHandler&& handler)
{
    return net::async_initiate<ReadHandler, void(error_code)>(
        [&s, &msg](auto handler)
        {
            error_code ec;
            read(s, msg, ec);
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
