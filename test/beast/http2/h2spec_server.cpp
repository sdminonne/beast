//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// h2spec conformance test server
//
// A standalone HTTP/2 (h2c) server that uses Beast's HTTP/2 primitives
// directly to implement a correct frame read loop. Designed to be tested
// with h2spec (https://github.com/summerwind/h2spec).
//
// Usage: h2spec_server [-p port]
//        Default port: 9080
//
// Then run: h2spec -h 127.0.0.1 -p 9080 --timeout 5
//
//------------------------------------------------------------------------------

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http2/frame.hpp>
#include <boost/beast/http2/settings.hpp>
#include <boost/beast/http2/error.hpp>
#include <boost/beast/http2/hpack/decoder.hpp>
#include <boost/beast/http2/hpack/encoder.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace beast = boost::beast;
namespace http = beast::http;
namespace http2 = beast::http2;
namespace hpack = http2::hpack;
namespace net = boost::asio;
using tcp = net::ip::tcp;

//------------------------------------------------------------------------------

// HTTP/2 client connection preface (RFC 9113 Section 3.4)
static constexpr char client_preface[] =
    "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
static constexpr std::size_t client_preface_size = 24;

// RFC 9113 error code values (for GOAWAY/RST_STREAM wire format)
static constexpr std::uint32_t H2_NO_ERROR           = 0x0;
static constexpr std::uint32_t H2_PROTOCOL_ERROR     = 0x1;
static constexpr std::uint32_t H2_FLOW_CONTROL_ERROR = 0x3;
static constexpr std::uint32_t H2_STREAM_CLOSED      = 0x5;
static constexpr std::uint32_t H2_FRAME_SIZE_ERROR   = 0x6;
static constexpr std::uint32_t H2_COMPRESSION_ERROR  = 0x9;

//------------------------------------------------------------------------------

// Stream states (RFC 9113 Section 5.1)
enum class sstate
{
    open,
    half_closed_remote,
    half_closed_local,
    closed
};

// Per-stream state
struct stream_info
{
    std::uint32_t id = 0;
    sstate state = sstate::open;
    bool headers_received = false;
    bool rst_closed = false; // true if closed via RST_STREAM
    std::int64_t expected_content_length = -1;
    std::int64_t received_content_length = 0;
    std::int32_t recv_window = http2::default_initial_window_size;
    std::int32_t send_window = http2::default_initial_window_size;

    // Pending response data for flow control
    std::string pending_body;
    std::size_t pending_offset = 0;
    bool response_headers_sent = false;

    bool has_pending_data() const
    {
        return response_headers_sent &&
            pending_offset < pending_body.size();
    }
};

//------------------------------------------------------------------------------

// Read exactly n bytes from a socket
static void
read_exact(
    tcp::socket& sock,
    std::uint8_t* buf,
    std::size_t n,
    beast::error_code& ec)
{
    std::size_t total = 0;
    while(total < n)
    {
        auto bytes = sock.read_some(
            net::buffer(buf + total, n - total), ec);
        if(ec)
            return;
        if(bytes == 0)
        {
            ec = net::error::eof;
            return;
        }
        total += bytes;
    }
}

//------------------------------------------------------------------------------

// Send a frame (header + payload)
static void
send_frame(
    tcp::socket& sock,
    http2::frame_type type,
    std::uint8_t flags,
    std::uint32_t stream_id,
    std::uint8_t const* payload,
    std::uint32_t payload_len,
    beast::error_code& ec)
{
    http2::frame_header fh;
    fh.length = payload_len;
    fh.type = type;
    fh.flags = flags;
    fh.stream_id = stream_id;

    std::uint8_t buf[http2::frame_header_size + 65536];
    http2::serialize_frame_header(buf, fh);

    if(payload_len > 0 && payload_len <= sizeof(buf) - http2::frame_header_size)
    {
        std::memcpy(buf + http2::frame_header_size, payload, payload_len);
        net::write(sock, net::buffer(buf, http2::frame_header_size + payload_len), ec);
    }
    else
    {
        net::write(sock, net::buffer(buf, http2::frame_header_size), ec);
        if(!ec && payload_len > 0)
            net::write(sock, net::buffer(payload, payload_len), ec);
    }
}

// Send SETTINGS frame
static void
send_settings(
    tcp::socket& sock,
    http2::settings const& s,
    beast::error_code& ec)
{
    std::uint8_t buf[36];
    auto n = http2::serialize_settings(buf, s);
    send_frame(sock, http2::frame_type::settings, 0, 0,
        buf, static_cast<std::uint32_t>(n), ec);
}

// Send SETTINGS ACK
static void
send_settings_ack(
    tcp::socket& sock,
    beast::error_code& ec)
{
    send_frame(sock, http2::frame_type::settings,
        http2::frame_flag::ack, 0, nullptr, 0, ec);
}

// Send PING ACK
static void
send_ping_ack(
    tcp::socket& sock,
    std::uint8_t const* opaque_data,
    beast::error_code& ec)
{
    send_frame(sock, http2::frame_type::ping,
        http2::frame_flag::ack, 0, opaque_data, 8, ec);
}

// Send GOAWAY frame
static void
send_goaway(
    tcp::socket& sock,
    std::uint32_t last_stream_id,
    std::uint32_t error_code_val,
    beast::error_code& ec)
{
    std::uint8_t payload[8];
    payload[0] = static_cast<std::uint8_t>((last_stream_id >> 24) & 0x7f);
    payload[1] = static_cast<std::uint8_t>((last_stream_id >> 16) & 0xff);
    payload[2] = static_cast<std::uint8_t>((last_stream_id >> 8) & 0xff);
    payload[3] = static_cast<std::uint8_t>(last_stream_id & 0xff);
    payload[4] = static_cast<std::uint8_t>((error_code_val >> 24) & 0xff);
    payload[5] = static_cast<std::uint8_t>((error_code_val >> 16) & 0xff);
    payload[6] = static_cast<std::uint8_t>((error_code_val >> 8) & 0xff);
    payload[7] = static_cast<std::uint8_t>(error_code_val & 0xff);
    send_frame(sock, http2::frame_type::goaway, 0, 0, payload, 8, ec);
}

// Send RST_STREAM frame
static void
send_rst_stream(
    tcp::socket& sock,
    std::uint32_t stream_id,
    std::uint32_t error_code_val,
    beast::error_code& ec)
{
    std::uint8_t payload[4];
    payload[0] = static_cast<std::uint8_t>((error_code_val >> 24) & 0xff);
    payload[1] = static_cast<std::uint8_t>((error_code_val >> 16) & 0xff);
    payload[2] = static_cast<std::uint8_t>((error_code_val >> 8) & 0xff);
    payload[3] = static_cast<std::uint8_t>(error_code_val & 0xff);
    send_frame(sock, http2::frame_type::rst_stream, 0, stream_id,
        payload, 4, ec);
}

// Send WINDOW_UPDATE frame
static void
send_window_update(
    tcp::socket& sock,
    std::uint32_t stream_id,
    std::uint32_t increment,
    beast::error_code& ec)
{
    std::uint8_t payload[4];
    payload[0] = static_cast<std::uint8_t>((increment >> 24) & 0x7f);
    payload[1] = static_cast<std::uint8_t>((increment >> 16) & 0xff);
    payload[2] = static_cast<std::uint8_t>((increment >> 8) & 0xff);
    payload[3] = static_cast<std::uint8_t>(increment & 0xff);
    send_frame(sock, http2::frame_type::window_update, 0, stream_id,
        payload, 4, ec);
}

//------------------------------------------------------------------------------

// HPACK decode handler that collects headers
struct header_collector : hpack::decode_handler
{
    std::vector<std::pair<std::string, std::string>> headers;

    void on_header(
        beast::string_view name,
        beast::string_view value,
        bool,
        beast::error_code&) override
    {
        headers.emplace_back(
            std::string(name.data(), name.size()),
            std::string(value.data(), value.size()));
    }
};

//------------------------------------------------------------------------------

// Validate request headers per RFC 9113 Section 8
// Returns true if valid, false if malformed
static bool
validate_request_headers(
    std::vector<std::pair<std::string, std::string>> const& headers,
    bool is_trailers)
{
    bool in_pseudo = true;
    bool has_method = false;
    bool has_scheme = false;
    bool has_path = false;
    bool has_authority = false;
    bool is_connect = false;

    for(auto const& h : headers)
    {
        if(h.first.empty())
            return false;

        if(h.first[0] == ':')
        {
            // Pseudo-headers must come before regular headers
            if(!in_pseudo)
                return false;

            // No pseudo-headers in trailers
            if(is_trailers)
                return false;

            if(h.first == ":method")
            {
                if(has_method)
                    return false; // duplicate
                has_method = true;
                is_connect = (h.second == "CONNECT");
            }
            else if(h.first == ":scheme")
            {
                if(has_scheme)
                    return false;
                has_scheme = true;
            }
            else if(h.first == ":path")
            {
                if(has_path)
                    return false;
                if(h.second.empty())
                    return false;
                has_path = true;
            }
            else if(h.first == ":authority")
            {
                if(has_authority)
                    return false;
                has_authority = true;
            }
            else if(h.first == ":status")
            {
                return false; // response pseudo-header in request
            }
            else
            {
                return false; // unknown pseudo-header
            }
        }
        else
        {
            in_pseudo = false;

            // Header field names must be lowercase (RFC 9113 Section 8.2)
            for(char c : h.first)
            {
                if(c >= 'A' && c <= 'Z')
                    return false;
            }

            // Connection-specific headers are prohibited
            // (RFC 9113 Section 8.2.2)
            if(h.first == "connection" ||
               h.first == "keep-alive" ||
               h.first == "proxy-connection" ||
               h.first == "transfer-encoding" ||
               h.first == "upgrade")
                return false;

            // TE must be "trailers" only
            if(h.first == "te" && h.second != "trailers")
                return false;
        }
    }

    if(!is_trailers)
    {
        if(!has_method)
            return false;

        if(is_connect)
        {
            // CONNECT: must have :authority, must NOT have :scheme or :path
            if(has_scheme || has_path)
                return false;
        }
        else
        {
            if(!has_scheme || !has_path)
                return false;
        }
    }

    return true;
}

// Extract content-length from headers (-1 if not present)
static std::int64_t
get_content_length(
    std::vector<std::pair<std::string, std::string>> const& headers)
{
    for(auto const& h : headers)
    {
        if(h.first == "content-length")
        {
            std::int64_t val = 0;
            for(char c : h.second)
            {
                if(c < '0' || c > '9')
                    return -1;
                val = val * 10 + (c - '0');
            }
            return val;
        }
    }
    return -1;
}

//------------------------------------------------------------------------------

// Determine if a stream ID is for an idle stream
// (never opened by the peer)
static bool
is_idle_stream(
    std::uint32_t stream_id,
    std::uint32_t last_peer_stream_id,
    std::unordered_map<std::uint32_t, stream_info> const& streams)
{
    // Client streams are odd; stream_id > last opened means idle
    if(stream_id > last_peer_stream_id)
        return true;
    // If it's <= last_peer_stream_id but not in the map,
    // it's been implicitly closed (not idle)
    return false;
}

//------------------------------------------------------------------------------

// Try to send pending DATA for a stream, respecting flow control.
// Returns true if all data was sent (or error), false if more data pending.
static bool
try_send_data(
    tcp::socket& sock,
    stream_info& si,
    std::int32_t& conn_send_window,
    beast::error_code& ec)
{
    while(si.has_pending_data())
    {
        std::size_t remaining =
            si.pending_body.size() - si.pending_offset;
        std::int32_t available = std::min(
            conn_send_window, si.send_window);
        if(available <= 0)
            return false; // must wait for WINDOW_UPDATE

        std::size_t to_send = std::min(
            remaining, static_cast<std::size_t>(available));

        bool is_last = (si.pending_offset + to_send >= si.pending_body.size());
        std::uint8_t flags = is_last ? http2::frame_flag::end_stream : 0;

        send_frame(sock, http2::frame_type::data, flags, si.id,
            reinterpret_cast<std::uint8_t const*>(
                si.pending_body.data() + si.pending_offset),
            static_cast<std::uint32_t>(to_send), ec);
        if(ec)
            return true;

        si.pending_offset += to_send;
        conn_send_window -= static_cast<std::int32_t>(to_send);
        si.send_window -= static_cast<std::int32_t>(to_send);
    }
    return true; // all data sent
}

// Send a 200 OK response with body "hello"
// Sends HEADERS immediately, then sends DATA respecting flow control.
// If flow control prevents sending all data, stores it as pending.
static void
send_response(
    tcp::socket& sock,
    stream_info& si,
    hpack::encoder& enc,
    std::int32_t& conn_send_window,
    beast::error_code& ec)
{
    std::uint8_t hdr_buf[4096];
    std::size_t hdr_len = 0;

    hdr_len += enc.encode_field(
        hdr_buf + hdr_len, sizeof(hdr_buf) - hdr_len,
        ":status", "200");
    hdr_len += enc.encode_field(
        hdr_buf + hdr_len, sizeof(hdr_buf) - hdr_len,
        "content-type", "text/plain");
    hdr_len += enc.encode_field(
        hdr_buf + hdr_len, sizeof(hdr_buf) - hdr_len,
        "content-length", "5");

    send_frame(sock, http2::frame_type::headers,
        http2::frame_flag::end_headers,
        si.id,
        hdr_buf, static_cast<std::uint32_t>(hdr_len), ec);
    if(ec)
        return;

    si.response_headers_sent = true;
    si.pending_body = "hello";
    si.pending_offset = 0;

    try_send_data(sock, si, conn_send_window, ec);
}

//------------------------------------------------------------------------------

static void
handle_connection(tcp::socket sock)
{
    beast::error_code ec;
    bool goaway_sent = false;

    auto close_conn = [&]()
    {
        if(!goaway_sent)
        {
            beast::error_code ignore;
            send_goaway(sock, 0, H2_NO_ERROR, ignore);
        }
        beast::error_code ignore;
        sock.shutdown(tcp::socket::shutdown_both, ignore);
        sock.close(ignore);
    };

    // --- Connection preface ---

    // Read the 24-byte client connection preface magic
    std::uint8_t preface_buf[client_preface_size];
    read_exact(sock, preface_buf, client_preface_size, ec);
    if(ec)
    {
        close_conn();
        return;
    }

    if(std::memcmp(preface_buf, client_preface, client_preface_size) != 0)
    {
        // Send GOAWAY before closing (h2spec expects this)
        send_goaway(sock, 0, H2_PROTOCOL_ERROR, ec);
        goaway_sent = true;
        close_conn();
        return;
    }

    // Read the client's SETTINGS frame (must be the first frame)
    std::uint8_t frame_hdr_buf[http2::frame_header_size];
    read_exact(sock, frame_hdr_buf, http2::frame_header_size, ec);
    if(ec)
    {
        close_conn();
        return;
    }

    http2::frame_header fh;
    http2::parse_frame_header(fh, frame_hdr_buf, ec);
    if(ec || fh.type != http2::frame_type::settings ||
       (fh.flags & http2::frame_flag::ack) || fh.stream_id != 0)
    {
        send_goaway(sock, 0, H2_PROTOCOL_ERROR, ec);
        goaway_sent = true;
        close_conn();
        return;
    }

    // Read settings payload
    http2::settings remote_settings;
    if(fh.length > 0)
    {
        std::vector<std::uint8_t> settings_payload(fh.length);
        read_exact(sock, settings_payload.data(), fh.length, ec);
        if(ec)
        {
            close_conn();
            return;
        }

        http2::parse_settings(
            remote_settings, settings_payload.data(), fh.length, ec);
        if(ec)
        {
            send_goaway(sock, 0, H2_PROTOCOL_ERROR, ec);
            goaway_sent = true;
            close_conn();
            return;
        }
    }

    // Send our server SETTINGS
    http2::settings local_settings;
    local_settings.max_concurrent_streams = 100;
    local_settings.enable_push = false;
    send_settings(sock, local_settings, ec);
    if(ec)
    {
        close_conn();
        return;
    }

    // Send SETTINGS ACK for the client's settings
    send_settings_ack(sock, ec);
    if(ec)
    {
        close_conn();
        return;
    }

    // --- Frame read loop ---

    hpack::decoder hpack_dec;
    hpack_dec.set_protocol_max_table_size(
        local_settings.header_table_size);
    hpack::encoder hpack_enc;
    std::unordered_map<std::uint32_t, stream_info> streams;
    std::uint32_t last_peer_stream_id = 0;
    std::int32_t conn_recv_window = http2::default_initial_window_size;
    std::int32_t conn_send_window = http2::default_initial_window_size;

    // CONTINUATION state
    bool expecting_continuation = false;
    std::uint32_t continuation_stream_id = 0;
    std::uint8_t continuation_flags = 0;
    std::vector<std::uint8_t> header_block_buf;

    // Lambda to process a completed header block (from HEADERS or CONTINUATION)
    auto process_headers = [&](
        std::uint32_t stream_id,
        std::uint8_t const* hdr_data,
        std::size_t hdr_len,
        std::uint8_t flags) -> bool
    {
        // Decode HPACK
        header_collector collector;
        hpack_dec.decode(hdr_data, hdr_len, collector, ec);
        if(ec)
        {
            send_goaway(sock, last_peer_stream_id,
                H2_COMPRESSION_ERROR, ec);
            goaway_sent = true;
            return false;
        }

        // Check if this stream already exists (trailers vs initial headers)
        auto it = streams.find(stream_id);
        bool is_trailers = (it != streams.end() && it->second.headers_received);

        // Validate request headers
        if(!validate_request_headers(collector.headers, is_trailers))
        {
            // Malformed request: send RST_STREAM PROTOCOL_ERROR
            send_rst_stream(sock, stream_id, H2_PROTOCOL_ERROR, ec);
            if(ec)
                return false;
            // Must still decode HPACK for state consistency, already done
            return true; // continue processing other streams
        }

        if(is_trailers)
        {
            // Trailers must have END_STREAM
            if(!(flags & http2::frame_flag::end_stream))
            {
                send_goaway(sock, last_peer_stream_id,
                    H2_PROTOCOL_ERROR, ec);
                goaway_sent = true;
                return false;
            }
            it->second.state = sstate::closed;
            return true;
        }

        // Create/update stream entry
        auto& si = streams[stream_id];
        si.id = stream_id;
        si.headers_received = true;
        si.send_window = static_cast<std::int32_t>(
            remote_settings.initial_window_size);
        si.expected_content_length = get_content_length(collector.headers);

        if(stream_id > last_peer_stream_id)
            last_peer_stream_id = stream_id;

        if(flags & http2::frame_flag::end_stream)
        {
            si.state = sstate::half_closed_remote;

            // Check content-length mismatch (expected > 0 but no data)
            if(si.expected_content_length > 0 &&
               si.received_content_length != si.expected_content_length)
            {
                send_rst_stream(sock, stream_id, H2_PROTOCOL_ERROR, ec);
                si.state = sstate::closed;
                return !ec;
            }
        }
        else
        {
            si.state = sstate::open;
        }

        // Send 200 OK response (flow-control-aware)
        send_response(sock, si, hpack_enc, conn_send_window, ec);
        if(ec)
            return false;

        // Update state after response:
        // If all data sent (END_STREAM sent), transition state
        if(!si.has_pending_data())
        {
            if(si.state == sstate::half_closed_remote)
                si.state = sstate::closed;
            else if(si.state == sstate::open)
                si.state = sstate::half_closed_local;
        }

        return true;
    };

    for(;;)
    {
        // Read frame header
        read_exact(sock, frame_hdr_buf, http2::frame_header_size, ec);
        if(ec)
            break;

        http2::parse_frame_header(fh, frame_hdr_buf, ec);
        if(ec)
            break;

        // Validate frame size against our local setting
        if(fh.length > local_settings.max_frame_size)
        {
            send_goaway(sock, last_peer_stream_id,
                H2_FRAME_SIZE_ERROR, ec);
            goaway_sent = true;
            break;
        }

        // Read payload
        std::vector<std::uint8_t> payload(fh.length);
        if(fh.length > 0)
        {
            read_exact(sock, payload.data(), fh.length, ec);
            if(ec)
                break;
        }

        // If expecting CONTINUATION, only CONTINUATION on same stream is valid
        if(expecting_continuation)
        {
            if(fh.type != http2::frame_type::continuation ||
               fh.stream_id != continuation_stream_id)
            {
                send_goaway(sock, last_peer_stream_id,
                    H2_PROTOCOL_ERROR, ec);
                goaway_sent = true;
                break;
            }

            header_block_buf.insert(header_block_buf.end(),
                payload.begin(), payload.end());

            if(fh.flags & http2::frame_flag::end_headers)
            {
                expecting_continuation = false;

                if(!process_headers(
                    continuation_stream_id,
                    header_block_buf.data(),
                    header_block_buf.size(),
                    continuation_flags))
                    break;

                header_block_buf.clear();
            }

            continue;
        }

        // Unknown frame types must be ignored (RFC 9113 Section 4.1)
        auto raw_type = static_cast<std::uint8_t>(fh.type);
        if(raw_type > static_cast<std::uint8_t>(
               http2::frame_type::continuation))
        {
            continue;
        }

        switch(fh.type)
        {
        //--------------------------------------------------------------
        case http2::frame_type::data:
        {
            if(fh.stream_id == 0)
            {
                send_goaway(sock, last_peer_stream_id,
                    H2_PROTOCOL_ERROR, ec);
                goaway_sent = true;
                break;
            }

            // Stream state validation
            if(is_idle_stream(fh.stream_id, last_peer_stream_id, streams))
            {
                send_goaway(sock, last_peer_stream_id,
                    H2_PROTOCOL_ERROR, ec);
                goaway_sent = true;
                break;
            }

            auto it = streams.find(fh.stream_id);
            if(it != streams.end())
            {
                if(it->second.state == sstate::half_closed_remote ||
                   it->second.state == sstate::closed)
                {
                    send_rst_stream(sock, fh.stream_id,
                        H2_STREAM_CLOSED, ec);
                    if(ec)
                        break;
                    // Must still account for flow control
                    conn_recv_window -= static_cast<std::int32_t>(fh.length);
                    continue;
                }
            }
            else
            {
                // Stream ID <= last_peer_stream_id but not in map
                // means it was closed implicitly
                send_rst_stream(sock, fh.stream_id,
                    H2_STREAM_CLOSED, ec);
                if(ec)
                    break;
                conn_recv_window -= static_cast<std::int32_t>(fh.length);
                continue;
            }

            // Parse padding
            std::size_t data_len = fh.length;
            if(fh.flags & http2::frame_flag::padded)
            {
                if(data_len < 1)
                {
                    send_goaway(sock, last_peer_stream_id,
                        H2_FRAME_SIZE_ERROR, ec);
                    goaway_sent = true;
                    break;
                }
                std::uint8_t pad_length = payload[0];
                if(pad_length >= data_len)
                {
                    send_goaway(sock, last_peer_stream_id,
                        H2_PROTOCOL_ERROR, ec);
                    goaway_sent = true;
                    break;
                }
                data_len = data_len - 1 - pad_length;
            }

            // Update flow control
            conn_recv_window -= static_cast<std::int32_t>(fh.length);
            if(it != streams.end())
            {
                it->second.recv_window -=
                    static_cast<std::int32_t>(fh.length);
                it->second.received_content_length +=
                    static_cast<std::int64_t>(data_len);
            }

            // Send connection WINDOW_UPDATE when window is low
            if(conn_recv_window <
               http2::default_initial_window_size / 2)
            {
                std::uint32_t incr = static_cast<std::uint32_t>(
                    http2::default_initial_window_size -
                    conn_recv_window);
                send_window_update(sock, 0, incr, ec);
                if(ec)
                    break;
                conn_recv_window = http2::default_initial_window_size;
            }

            // Send stream WINDOW_UPDATE when window is low
            if(it != streams.end() &&
               it->second.recv_window <
                   http2::default_initial_window_size / 2)
            {
                std::uint32_t incr = static_cast<std::uint32_t>(
                    http2::default_initial_window_size -
                    it->second.recv_window);
                send_window_update(sock, fh.stream_id, incr, ec);
                if(ec)
                    break;
                it->second.recv_window =
                    http2::default_initial_window_size;
            }

            if(fh.flags & http2::frame_flag::end_stream)
            {
                if(it != streams.end())
                {
                    // Check content-length mismatch
                    if(it->second.expected_content_length >= 0 &&
                       it->second.received_content_length !=
                           it->second.expected_content_length)
                    {
                        send_rst_stream(sock, fh.stream_id,
                            H2_PROTOCOL_ERROR, ec);
                        it->second.state = sstate::closed;
                        if(ec)
                            break;
                        continue;
                    }

                    if(it->second.state == sstate::open)
                        it->second.state = sstate::half_closed_remote;
                    else if(it->second.state == sstate::half_closed_local)
                        it->second.state = sstate::closed;
                }
            }

            continue;
        }

        //--------------------------------------------------------------
        case http2::frame_type::headers:
        {
            if(fh.stream_id == 0)
            {
                send_goaway(sock, last_peer_stream_id,
                    H2_PROTOCOL_ERROR, ec);
                goaway_sent = true;
                break;
            }

            // Check for even stream ID from client
            if((fh.stream_id & 1) == 0)
            {
                send_goaway(sock, last_peer_stream_id,
                    H2_PROTOCOL_ERROR, ec);
                goaway_sent = true;
                break;
            }

            // Stream state validation
            auto it = streams.find(fh.stream_id);
            if(it != streams.end())
            {
                // Existing stream
                if(it->second.state == sstate::half_closed_remote ||
                   it->second.state == sstate::closed)
                {
                    // Must still decode HPACK to keep decoder state
                    // consistent, but reject the frame.
                    // Closed via END_STREAM → connection error (GOAWAY)
                    // Closed via RST_STREAM → stream error (RST_STREAM)
                    if(it->second.state == sstate::closed &&
                       !it->second.rst_closed)
                    {
                        // Need to decode HPACK before sending GOAWAY
                        std::uint8_t const* hdr_d = payload.data();
                        std::size_t hdr_l = fh.length;
                        if(fh.flags & http2::frame_flag::padded)
                        {
                            if(hdr_l >= 1)
                            {
                                std::uint8_t pl = hdr_d[0];
                                hdr_d += 1; hdr_l -= 1;
                                if(pl < hdr_l) hdr_l -= pl;
                            }
                        }
                        if(fh.flags & http2::frame_flag::priority)
                        {
                            if(hdr_l >= 5)
                            { hdr_d += 5; hdr_l -= 5; }
                        }
                        if(fh.flags & http2::frame_flag::end_headers)
                        {
                            header_collector dummy;
                            beast::error_code dec_ec;
                            hpack_dec.decode(hdr_d, hdr_l,
                                dummy, dec_ec);
                        }
                        send_goaway(sock, last_peer_stream_id,
                            H2_STREAM_CLOSED, ec);
                        goaway_sent = true;
                        break;
                    }

                    send_rst_stream(sock, fh.stream_id,
                        H2_STREAM_CLOSED, ec);

                    // Still need to decode HPACK for state consistency
                    // Parse padding + priority to find header block
                    std::uint8_t const* hdr_data = payload.data();
                    std::size_t hdr_len = fh.length;
                    if(fh.flags & http2::frame_flag::padded)
                    {
                        if(hdr_len >= 1)
                        {
                            std::uint8_t pl = hdr_data[0];
                            hdr_data += 1;
                            hdr_len -= 1;
                            if(pl < hdr_len)
                                hdr_len -= pl;
                        }
                    }
                    if(fh.flags & http2::frame_flag::priority)
                    {
                        if(hdr_len >= 5)
                        {
                            hdr_data += 5;
                            hdr_len -= 5;
                        }
                    }
                    if(fh.flags & http2::frame_flag::end_headers)
                    {
                        header_collector dummy;
                        beast::error_code dec_ec;
                        hpack_dec.decode(hdr_data, hdr_len,
                            dummy, dec_ec);
                        if(dec_ec)
                        {
                            send_goaway(sock, last_peer_stream_id,
                                H2_COMPRESSION_ERROR, ec);
                            goaway_sent = true;
                            break;
                        }
                    }
                    // If not END_HEADERS, we need CONTINUATION frames
                    // to maintain HPACK state. Set up continuation but
                    // we won't process the headers.

                    if(ec)
                        break;
                    continue;
                }

                if(it->second.state == sstate::half_closed_local)
                {
                    // Trailers from client on half_closed_local:
                    // this is allowed (client hasn't sent END_STREAM yet)
                }
                else if(it->second.state == sstate::open &&
                        it->second.headers_received)
                {
                    // Second HEADERS on open stream - only valid as
                    // trailers with END_STREAM
                    if(!(fh.flags & http2::frame_flag::end_stream))
                    {
                        send_rst_stream(sock, fh.stream_id,
                            H2_PROTOCOL_ERROR, ec);
                        // Still decode HPACK
                        std::uint8_t const* hdr_data = payload.data();
                        std::size_t hdr_len = fh.length;
                        if(fh.flags & http2::frame_flag::padded)
                        {
                            if(hdr_len >= 1)
                            {
                                std::uint8_t pl = hdr_data[0];
                                hdr_data += 1;
                                hdr_len -= 1;
                                if(pl < hdr_len)
                                    hdr_len -= pl;
                            }
                        }
                        if(fh.flags & http2::frame_flag::priority)
                        {
                            if(hdr_len >= 5)
                            {
                                hdr_data += 5;
                                hdr_len -= 5;
                            }
                        }
                        if(fh.flags & http2::frame_flag::end_headers)
                        {
                            header_collector dummy;
                            beast::error_code dec_ec;
                            hpack_dec.decode(hdr_data, hdr_len,
                                dummy, dec_ec);
                            if(dec_ec)
                            {
                                send_goaway(sock, last_peer_stream_id,
                                    H2_COMPRESSION_ERROR, ec);
                                goaway_sent = true;
                                break;
                            }
                        }
                        if(ec)
                            break;
                        continue;
                    }
                }
            }
            else if(fh.stream_id <= last_peer_stream_id)
            {
                // Stream was closed (not in map, but ID was used before)
                send_goaway(sock, last_peer_stream_id,
                    H2_STREAM_CLOSED, ec);
                goaway_sent = true;
                // Decode HPACK for consistency
                std::uint8_t const* hdr_data = payload.data();
                std::size_t hdr_len = fh.length;
                if(fh.flags & http2::frame_flag::padded)
                {
                    if(hdr_len >= 1)
                    {
                        std::uint8_t pl = hdr_data[0];
                        hdr_data += 1;
                        hdr_len -= 1;
                        if(pl < hdr_len)
                            hdr_len -= pl;
                    }
                }
                if(fh.flags & http2::frame_flag::priority)
                {
                    if(hdr_len >= 5)
                    {
                        hdr_data += 5;
                        hdr_len -= 5;
                    }
                }
                if(fh.flags & http2::frame_flag::end_headers)
                {
                    header_collector dummy;
                    beast::error_code dec_ec;
                    hpack_dec.decode(hdr_data, hdr_len, dummy, dec_ec);
                }
                break;
            }
            else
            {
                // New stream - check stream ID is monotonically increasing
                // and odd (already checked even above)
                // Stream ID must be > last_peer_stream_id
                // (This case handles it since we're in the else branch
                //  of stream_id <= last_peer_stream_id)
            }

            // Parse padding
            std::uint8_t const* hdr_data = payload.data();
            std::size_t hdr_len = fh.length;

            if(fh.flags & http2::frame_flag::padded)
            {
                if(hdr_len < 1)
                {
                    send_goaway(sock, last_peer_stream_id,
                        H2_FRAME_SIZE_ERROR, ec);
                    goaway_sent = true;
                    break;
                }
                std::uint8_t pad_length = hdr_data[0];
                hdr_data += 1;
                hdr_len -= 1;

                if(pad_length >= hdr_len)
                {
                    send_goaway(sock, last_peer_stream_id,
                        H2_PROTOCOL_ERROR, ec);
                    goaway_sent = true;
                    break;
                }
                hdr_len -= pad_length;
            }

            // Parse priority and check self-dependency
            if(fh.flags & http2::frame_flag::priority)
            {
                if(hdr_len < 5)
                {
                    send_goaway(sock, last_peer_stream_id,
                        H2_FRAME_SIZE_ERROR, ec);
                    goaway_sent = true;
                    break;
                }

                // Check self-dependency
                std::uint32_t dep_stream =
                    (static_cast<std::uint32_t>(hdr_data[0] & 0x7f) << 24) |
                    (static_cast<std::uint32_t>(hdr_data[1]) << 16) |
                    (static_cast<std::uint32_t>(hdr_data[2]) << 8) |
                    static_cast<std::uint32_t>(hdr_data[3]);

                if(dep_stream == fh.stream_id)
                {
                    // Self-dependency is a stream error (PROTOCOL_ERROR)
                    send_rst_stream(sock, fh.stream_id,
                        H2_PROTOCOL_ERROR, ec);
                    // Decode HPACK for consistency
                    if(fh.flags & http2::frame_flag::end_headers)
                    {
                        header_collector dummy;
                        beast::error_code dec_ec;
                        hpack_dec.decode(hdr_data + 5, hdr_len - 5,
                            dummy, dec_ec);
                        if(dec_ec)
                        {
                            send_goaway(sock, last_peer_stream_id,
                                H2_COMPRESSION_ERROR, ec);
                            goaway_sent = true;
                            break;
                        }
                    }
                    // Still create stream entry
                    auto& si = streams[fh.stream_id];
                    si.id = fh.stream_id;
                    si.headers_received = true;
                    si.state = sstate::closed;
                    if(fh.stream_id > last_peer_stream_id)
                        last_peer_stream_id = fh.stream_id;
                    if(ec)
                        break;
                    continue;
                }

                hdr_data += 5;
                hdr_len -= 5;
            }

            if(fh.flags & http2::frame_flag::end_headers)
            {
                if(!process_headers(fh.stream_id,
                    hdr_data, hdr_len, fh.flags))
                    break;
            }
            else
            {
                // Need CONTINUATION frames
                expecting_continuation = true;
                continuation_stream_id = fh.stream_id;
                continuation_flags = fh.flags;
                header_block_buf.assign(hdr_data, hdr_data + hdr_len);
            }

            continue;
        }

        //--------------------------------------------------------------
        case http2::frame_type::priority:
        {
            if(fh.stream_id == 0)
            {
                send_goaway(sock, last_peer_stream_id,
                    H2_PROTOCOL_ERROR, ec);
                goaway_sent = true;
                break;
            }

            if(fh.length != 5)
            {
                send_goaway(sock, last_peer_stream_id,
                    H2_FRAME_SIZE_ERROR, ec);
                goaway_sent = true;
                break;
            }

            // Check self-dependency
            std::uint32_t dep_stream =
                (static_cast<std::uint32_t>(payload[0] & 0x7f) << 24) |
                (static_cast<std::uint32_t>(payload[1]) << 16) |
                (static_cast<std::uint32_t>(payload[2]) << 8) |
                static_cast<std::uint32_t>(payload[3]);

            if(dep_stream == fh.stream_id)
            {
                send_rst_stream(sock, fh.stream_id,
                    H2_PROTOCOL_ERROR, ec);
                if(ec)
                    break;
                continue;
            }

            // Accept and ignore
            continue;
        }

        //--------------------------------------------------------------
        case http2::frame_type::rst_stream:
        {
            if(fh.stream_id == 0)
            {
                send_goaway(sock, last_peer_stream_id,
                    H2_PROTOCOL_ERROR, ec);
                goaway_sent = true;
                break;
            }

            if(fh.length != 4)
            {
                send_goaway(sock, last_peer_stream_id,
                    H2_FRAME_SIZE_ERROR, ec);
                goaway_sent = true;
                break;
            }

            // RST_STREAM on idle stream is a protocol error
            if(is_idle_stream(fh.stream_id, last_peer_stream_id, streams))
            {
                send_goaway(sock, last_peer_stream_id,
                    H2_PROTOCOL_ERROR, ec);
                goaway_sent = true;
                break;
            }

            // Mark stream as closed (via RST_STREAM)
            auto it = streams.find(fh.stream_id);
            if(it != streams.end())
            {
                it->second.state = sstate::closed;
                it->second.rst_closed = true;
            }

            continue;
        }

        //--------------------------------------------------------------
        case http2::frame_type::settings:
        {
            if(fh.stream_id != 0)
            {
                send_goaway(sock, last_peer_stream_id,
                    H2_PROTOCOL_ERROR, ec);
                goaway_sent = true;
                break;
            }

            if(fh.flags & http2::frame_flag::ack)
            {
                if(fh.length != 0)
                {
                    send_goaway(sock, last_peer_stream_id,
                        H2_FRAME_SIZE_ERROR, ec);
                    goaway_sent = true;
                    break;
                }
                continue;
            }

            if(fh.length % 6 != 0)
            {
                send_goaway(sock, last_peer_stream_id,
                    H2_FRAME_SIZE_ERROR, ec);
                goaway_sent = true;
                break;
            }

            http2::settings new_settings = remote_settings;
            http2::parse_settings(
                new_settings, payload.data(), fh.length, ec);
            if(ec)
            {
                auto err_code =
                    (ec == http2::error::flow_control_error)
                    ? H2_FLOW_CONTROL_ERROR
                    : H2_PROTOCOL_ERROR;
                ec = {};
                send_goaway(sock, last_peer_stream_id, err_code, ec);
                goaway_sent = true;
                break;
            }

            // Adjust stream windows on initial_window_size change
            if(new_settings.initial_window_size !=
               remote_settings.initial_window_size)
            {
                std::int32_t delta = static_cast<std::int32_t>(
                    new_settings.initial_window_size) -
                    static_cast<std::int32_t>(
                        remote_settings.initial_window_size);
                for(auto& kv : streams)
                {
                    if(kv.second.state == sstate::closed)
                        continue;
                    std::int64_t new_window =
                        static_cast<std::int64_t>(
                            kv.second.send_window) + delta;
                    if(new_window > http2::max_window_size)
                    {
                        send_goaway(sock, last_peer_stream_id,
                            H2_FLOW_CONTROL_ERROR, ec);
                        goaway_sent = true;
                        break;
                    }
                    kv.second.send_window =
                        static_cast<std::int32_t>(new_window);
                }
                if(goaway_sent)
                    break;
            }

            remote_settings = new_settings;

            // Try to flush pending data after window changes
            for(auto& kv : streams)
            {
                if(kv.second.has_pending_data() &&
                   kv.second.send_window > 0)
                {
                    bool done = try_send_data(
                        sock, kv.second,
                        conn_send_window, ec);
                    if(ec)
                        break;
                    if(done && !kv.second.has_pending_data())
                    {
                        if(kv.second.state == sstate::half_closed_remote)
                            kv.second.state = sstate::closed;
                        else if(kv.second.state == sstate::open)
                            kv.second.state = sstate::half_closed_local;
                    }
                }
            }
            if(ec)
                break;

            // ACK the settings
            send_settings_ack(sock, ec);
            if(ec)
                break;

            continue;
        }

        //--------------------------------------------------------------
        case http2::frame_type::ping:
        {
            if(fh.stream_id != 0)
            {
                send_goaway(sock, last_peer_stream_id,
                    H2_PROTOCOL_ERROR, ec);
                goaway_sent = true;
                break;
            }

            if(fh.length != 8)
            {
                send_goaway(sock, last_peer_stream_id,
                    H2_FRAME_SIZE_ERROR, ec);
                goaway_sent = true;
                break;
            }

            if(fh.flags & http2::frame_flag::ack)
                continue;

            send_ping_ack(sock, payload.data(), ec);
            if(ec)
                break;

            continue;
        }

        //--------------------------------------------------------------
        case http2::frame_type::goaway:
        {
            if(fh.stream_id != 0)
            {
                send_goaway(sock, last_peer_stream_id,
                    H2_PROTOCOL_ERROR, ec);
                goaway_sent = true;
                break;
            }

            // Accept GOAWAY gracefully and close
            goaway_sent = false; // so we send GOAWAY NO_ERROR in close
            break;
        }

        //--------------------------------------------------------------
        case http2::frame_type::window_update:
        {
            if(fh.length != 4)
            {
                send_goaway(sock, last_peer_stream_id,
                    H2_FRAME_SIZE_ERROR, ec);
                goaway_sent = true;
                break;
            }

            // Stream-level: check for idle stream
            if(fh.stream_id != 0 &&
               is_idle_stream(fh.stream_id, last_peer_stream_id, streams))
            {
                send_goaway(sock, last_peer_stream_id,
                    H2_PROTOCOL_ERROR, ec);
                goaway_sent = true;
                break;
            }

            std::uint32_t increment =
                (static_cast<std::uint32_t>(payload[0] & 0x7f) << 24) |
                (static_cast<std::uint32_t>(payload[1]) << 16) |
                (static_cast<std::uint32_t>(payload[2]) << 8) |
                static_cast<std::uint32_t>(payload[3]);

            if(increment == 0)
            {
                if(fh.stream_id == 0)
                {
                    send_goaway(sock, last_peer_stream_id,
                        H2_PROTOCOL_ERROR, ec);
                    goaway_sent = true;
                }
                else
                {
                    send_rst_stream(sock, fh.stream_id,
                        H2_PROTOCOL_ERROR, ec);
                }
                break;
            }

            if(fh.stream_id == 0)
            {
                std::int64_t new_window =
                    static_cast<std::int64_t>(conn_send_window) +
                    increment;
                if(new_window > http2::max_window_size)
                {
                    send_goaway(sock, last_peer_stream_id,
                        H2_FLOW_CONTROL_ERROR, ec);
                    goaway_sent = true;
                    break;
                }
                conn_send_window =
                    static_cast<std::int32_t>(new_window);

                // Try to flush pending data for all streams
                for(auto& kv : streams)
                {
                    if(kv.second.has_pending_data())
                    {
                        bool done = try_send_data(
                            sock, kv.second,
                            conn_send_window, ec);
                        if(ec)
                            break;
                        if(done && !kv.second.has_pending_data())
                        {
                            // END_STREAM sent, update state
                            if(kv.second.state == sstate::half_closed_remote)
                                kv.second.state = sstate::closed;
                            else if(kv.second.state == sstate::open)
                                kv.second.state = sstate::half_closed_local;
                        }
                    }
                }
            }
            else
            {
                auto it = streams.find(fh.stream_id);
                if(it != streams.end())
                {
                    std::int64_t new_window =
                        static_cast<std::int64_t>(
                            it->second.send_window) + increment;
                    if(new_window > http2::max_window_size)
                    {
                        send_rst_stream(sock, fh.stream_id,
                            H2_FLOW_CONTROL_ERROR, ec);
                        if(ec)
                            break;
                        continue;
                    }
                    it->second.send_window =
                        static_cast<std::int32_t>(new_window);

                    // Try to flush pending data for this stream
                    if(it->second.has_pending_data())
                    {
                        bool done = try_send_data(
                            sock, it->second,
                            conn_send_window, ec);
                        if(ec)
                            break;
                        if(done && !it->second.has_pending_data())
                        {
                            if(it->second.state == sstate::half_closed_remote)
                                it->second.state = sstate::closed;
                            else if(it->second.state == sstate::open)
                                it->second.state = sstate::half_closed_local;
                        }
                    }
                }
            }

            if(ec)
                break;
            continue;
        }

        //--------------------------------------------------------------
        case http2::frame_type::continuation:
        {
            // CONTINUATION without preceding HEADERS/CONTINUATION
            // without END_HEADERS is a protocol error
            send_goaway(sock, last_peer_stream_id,
                H2_PROTOCOL_ERROR, ec);
            goaway_sent = true;
            break;
        }

        //--------------------------------------------------------------
        case http2::frame_type::push_promise:
        {
            // Clients should not send PUSH_PROMISE to servers
            send_goaway(sock, last_peer_stream_id,
                H2_PROTOCOL_ERROR, ec);
            goaway_sent = true;
            break;
        }

        } // switch

        if(goaway_sent || ec ||
           fh.type == http2::frame_type::goaway)
            break;
    }

    close_conn();
}

//------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    unsigned short port = 9080;

    for(int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if((arg == "-p" || arg == "--port") && i + 1 < argc)
        {
            port = static_cast<unsigned short>(
                std::atoi(argv[++i]));
        }
        else if(arg == "-h" || arg == "--help")
        {
            std::cout <<
                "Usage: h2spec_server [-p port]\n"
                "  -p, --port PORT   Listen port (default: 9080)\n"
                "  -h, --help        Show this help\n";
            return 0;
        }
    }

    try
    {
        net::io_context ioc;

        tcp::acceptor acceptor(
            ioc, tcp::endpoint(
                net::ip::make_address("127.0.0.1"), port));
        acceptor.set_option(net::socket_base::reuse_address(true));

        std::cout << "h2spec_server listening on 127.0.0.1:"
                  << port << "\n";

        for(;;)
        {
            tcp::socket sock(ioc);
            beast::error_code ec;
            acceptor.accept(sock, ec);
            if(ec)
                continue;

            sock.set_option(tcp::no_delay(true), ec);

            // Handle each connection in its own thread
            std::thread(
                [s = std::move(sock)]() mutable
                {
                    handle_connection(std::move(s));
                }).detach();
        }
    }
    catch(std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
