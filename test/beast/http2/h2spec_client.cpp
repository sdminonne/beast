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
// h2spec client conformance test
//
// A standalone test program that embeds a mock HTTP/2 server and connects a
// Beast connection<tcp::socket> client to it, verifying correct client-side
// protocol behavior.
//
// Usage: h2spec_client [-p port]
//        Default port: 9081
//
//------------------------------------------------------------------------------

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http2/connection.hpp>
#include <boost/beast/http2/frame.hpp>
#include <boost/beast/http2/settings.hpp>
#include <boost/beast/http2/error.hpp>
#include <boost/beast/http2/hpack/decoder.hpp>
#include <boost/beast/http2/hpack/encoder.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
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
static constexpr std::uint32_t H2_NO_ERROR = 0x0;
static constexpr std::uint32_t H2_CANCEL   = 0x8;

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

// Send PING frame
static void
send_ping(
    tcp::socket& sock,
    std::uint8_t const* opaque_data,
    beast::error_code& ec)
{
    send_frame(sock, http2::frame_type::ping,
        0, 0, opaque_data, 8, ec);
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

    std::string find(std::string const& name) const
    {
        for(auto const& h : headers)
            if(h.first == name)
                return h.second;
        return {};
    }
};

//------------------------------------------------------------------------------

// Mock server: perform the server side of the HTTP/2 handshake.
// Reads client preface + SETTINGS, sends server SETTINGS + SETTINGS ACK,
// reads client SETTINGS ACK.
static bool
mock_server_handshake(
    tcp::socket& sock,
    http2::settings const& server_settings,
    beast::error_code& ec)
{
    // 1. Read client connection preface magic (24 bytes)
    std::uint8_t preface_buf[client_preface_size];
    read_exact(sock, preface_buf, client_preface_size, ec);
    if(ec)
        return false;
    if(std::memcmp(preface_buf, client_preface, client_preface_size) != 0)
        return false;

    // 2. Read client SETTINGS frame
    std::uint8_t frame_hdr_buf[http2::frame_header_size];
    read_exact(sock, frame_hdr_buf, http2::frame_header_size, ec);
    if(ec)
        return false;

    http2::frame_header fh;
    http2::parse_frame_header(fh, frame_hdr_buf, ec);
    if(ec || fh.type != http2::frame_type::settings ||
       (fh.flags & http2::frame_flag::ack) || fh.stream_id != 0)
        return false;

    // Read and discard client settings payload
    if(fh.length > 0)
    {
        std::vector<std::uint8_t> payload(fh.length);
        read_exact(sock, payload.data(), fh.length, ec);
        if(ec)
            return false;
    }

    // 3. Send server SETTINGS
    send_settings(sock, server_settings, ec);
    if(ec)
        return false;

    // 4. Send SETTINGS ACK for client settings
    send_settings_ack(sock, ec);
    if(ec)
        return false;

    // 5. Read client SETTINGS ACK
    read_exact(sock, frame_hdr_buf, http2::frame_header_size, ec);
    if(ec)
        return false;

    http2::parse_frame_header(fh, frame_hdr_buf, ec);
    if(ec)
        return false;

    if(fh.type != http2::frame_type::settings ||
       !(fh.flags & http2::frame_flag::ack))
        return false;

    // Consume any payload (should be 0)
    if(fh.length > 0)
    {
        std::vector<std::uint8_t> payload(fh.length);
        read_exact(sock, payload.data(), fh.length, ec);
        if(ec)
            return false;
    }

    return true;
}

// Read the next frame from the socket. Returns false on error.
static bool
read_frame(
    tcp::socket& sock,
    http2::frame_header& fh,
    std::vector<std::uint8_t>& payload,
    beast::error_code& ec)
{
    std::uint8_t hdr_buf[http2::frame_header_size];
    read_exact(sock, hdr_buf, http2::frame_header_size, ec);
    if(ec)
        return false;

    http2::parse_frame_header(fh, hdr_buf, ec);
    if(ec)
        return false;

    payload.resize(fh.length);
    if(fh.length > 0)
    {
        read_exact(sock, payload.data(), fh.length, ec);
        if(ec)
            return false;
    }

    return true;
}

//------------------------------------------------------------------------------

// Test result tracking
struct test_result
{
    int number;
    std::string name;
    bool passed;
    std::string detail;
};

static std::vector<test_result> results;

static void
report(int num, std::string const& name, bool passed,
       std::string const& detail = {})
{
    results.push_back({num, name, passed, detail});

    // Pad the name+dots to align output
    std::string line = std::to_string(num) + ". " + name + " ";
    while(line.size() < 52)
        line += '.';
    line += ' ';

    if(passed)
        line += "PASSED";
    else
    {
        line += "FAILED";
        if(!detail.empty())
            line += " (" + detail + ")";
    }
    std::cout << line << "\n";
}

//------------------------------------------------------------------------------
//
// Test 1: Connection preface & SETTINGS exchange
//
//------------------------------------------------------------------------------
static void
test_connection_preface(unsigned short port)
{
    net::io_context ioc;

    // Start mock server thread
    tcp::acceptor acceptor(ioc,
        tcp::endpoint(net::ip::make_address("127.0.0.1"), port));
    acceptor.set_option(net::socket_base::reuse_address(true));

    std::atomic<bool> server_ok{false};

    std::thread server_thread([&]()
    {
        beast::error_code ec;
        tcp::socket sock(ioc);
        acceptor.accept(sock, ec);
        if(ec) return;

        http2::settings srv_settings;
        srv_settings.max_concurrent_streams = 100;
        srv_settings.enable_push = false;

        if(!mock_server_handshake(sock, srv_settings, ec))
            return;

        server_ok = true;

        // Send GOAWAY(NO_ERROR) and close
        send_goaway(sock, 0, H2_NO_ERROR, ec);

        beast::error_code ignore;
        sock.shutdown(tcp::socket::shutdown_both, ignore);
        sock.close(ignore);
    });

    // Client side
    bool passed = false;
    std::string detail;
    try
    {
        net::io_context client_ioc;
        tcp::socket client_sock(client_ioc);
        client_sock.connect(
            tcp::endpoint(net::ip::make_address("127.0.0.1"), port));

        http2::connection<tcp::socket&> conn(client_sock);
        beast::error_code ec;
        conn.handshake(ec);

        if(!ec && conn.is_open())
        {
            auto const& rs = conn.remote_settings();
            if(rs.max_concurrent_streams == 100 && rs.enable_push == false)
                passed = true;
            else
            {
                detail = "settings mismatch: max_concurrent_streams=" +
                    std::to_string(rs.max_concurrent_streams) +
                    " enable_push=" + std::to_string(rs.enable_push);
            }
        }
        else
        {
            detail = "handshake failed: " + ec.message();
        }
    }
    catch(std::exception const& e)
    {
        detail = std::string("exception: ") + e.what();
    }

    server_thread.join();
    acceptor.close();
    report(1, "Connection preface & SETTINGS exchange", passed, detail);
}

//------------------------------------------------------------------------------
//
// Test 2: Basic request/response (raw frame I/O)
//
//------------------------------------------------------------------------------
static void
test_basic_request_response(unsigned short port)
{
    net::io_context ioc;

    tcp::acceptor acceptor(ioc,
        tcp::endpoint(net::ip::make_address("127.0.0.1"), port));
    acceptor.set_option(net::socket_base::reuse_address(true));

    std::thread server_thread([&]()
    {
        beast::error_code ec;
        tcp::socket sock(ioc);
        acceptor.accept(sock, ec);
        if(ec) return;

        http2::settings srv_settings;
        srv_settings.max_concurrent_streams = 100;
        srv_settings.enable_push = false;

        if(!mock_server_handshake(sock, srv_settings, ec))
            return;

        // Read client HEADERS frame on stream 1
        http2::frame_header fh;
        std::vector<std::uint8_t> payload;
        if(!read_frame(sock, fh, payload, ec))
            return;

        if(fh.type != http2::frame_type::headers || fh.stream_id != 1)
            return;

        // Decode HPACK to verify request headers
        hpack::decoder dec;
        header_collector collector;
        dec.decode(payload.data(), payload.size(), collector, ec);
        if(ec)
            return;

        // Verify :method and :path
        auto method = collector.find(":method");
        auto path = collector.find(":path");
        if(method != "GET" || path != "/hello")
            return;

        // Send response HEADERS (:status 200, content-type, content-length)
        hpack::encoder enc;
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
            1, hdr_buf, static_cast<std::uint32_t>(hdr_len), ec);
        if(ec)
            return;

        // Send DATA "hello" with END_STREAM
        std::uint8_t body[] = {'h', 'e', 'l', 'l', 'o'};
        send_frame(sock, http2::frame_type::data,
            http2::frame_flag::end_stream,
            1, body, 5, ec);
        if(ec)
            return;

        // Send GOAWAY
        send_goaway(sock, 1, H2_NO_ERROR, ec);

        beast::error_code ignore;
        sock.shutdown(tcp::socket::shutdown_both, ignore);
        sock.close(ignore);
    });

    // Client side: handshake, send raw HEADERS, read response frames
    bool passed = false;
    std::string detail;
    try
    {
        net::io_context client_ioc;
        tcp::socket client_sock(client_ioc);
        client_sock.connect(
            tcp::endpoint(net::ip::make_address("127.0.0.1"), port));

        http2::connection<tcp::socket&> conn(client_sock);
        beast::error_code ec;
        conn.handshake(ec);
        if(ec)
        {
            detail = "handshake failed: " + ec.message();
            server_thread.join();
            acceptor.close();
            report(2, "Basic request/response", false, detail);
            return;
        }

        // Send HEADERS frame for GET /hello on stream 1 (raw)
        hpack::encoder client_enc;
        std::uint8_t hdr_buf[4096];
        std::size_t hdr_len = 0;

        hdr_len += client_enc.encode_field(
            hdr_buf + hdr_len, sizeof(hdr_buf) - hdr_len,
            ":method", "GET");
        hdr_len += client_enc.encode_field(
            hdr_buf + hdr_len, sizeof(hdr_buf) - hdr_len,
            ":path", "/hello");
        hdr_len += client_enc.encode_field(
            hdr_buf + hdr_len, sizeof(hdr_buf) - hdr_len,
            ":scheme", "http");
        hdr_len += client_enc.encode_field(
            hdr_buf + hdr_len, sizeof(hdr_buf) - hdr_len,
            ":authority", "127.0.0.1");

        // Build and send raw frame through underlying socket
        http2::frame_header req_fh;
        req_fh.length = static_cast<std::uint32_t>(hdr_len);
        req_fh.type = http2::frame_type::headers;
        req_fh.flags = http2::frame_flag::end_headers |
                       http2::frame_flag::end_stream;
        req_fh.stream_id = 1;

        std::uint8_t frame_buf[http2::frame_header_size];
        http2::serialize_frame_header(frame_buf, req_fh);
        net::write(client_sock, net::buffer(frame_buf, http2::frame_header_size), ec);
        if(ec)
        {
            detail = "send header hdr failed: " + ec.message();
            server_thread.join();
            acceptor.close();
            report(2, "Basic request/response", false, detail);
            return;
        }
        net::write(client_sock, net::buffer(hdr_buf, hdr_len), ec);
        if(ec)
        {
            detail = "send header payload failed: " + ec.message();
            server_thread.join();
            acceptor.close();
            report(2, "Basic request/response", false, detail);
            return;
        }

        // Read response frames (raw reads from socket)
        // We expect: HEADERS, DATA, GOAWAY
        hpack::decoder client_dec;
        std::string response_status;
        std::string response_body;

        for(int i = 0; i < 10; ++i) // safety limit
        {
            std::uint8_t rhdr[http2::frame_header_size];
            read_exact(client_sock, rhdr, http2::frame_header_size, ec);
            if(ec)
                break;

            http2::frame_header rfh;
            http2::parse_frame_header(rfh, rhdr, ec);
            if(ec)
                break;

            std::vector<std::uint8_t> rpayload(rfh.length);
            if(rfh.length > 0)
            {
                read_exact(client_sock, rpayload.data(), rfh.length, ec);
                if(ec)
                    break;
            }

            if(rfh.type == http2::frame_type::headers && rfh.stream_id == 1)
            {
                header_collector col;
                client_dec.decode(rpayload.data(), rpayload.size(), col, ec);
                if(ec)
                    break;
                response_status = col.find(":status");
            }
            else if(rfh.type == http2::frame_type::data && rfh.stream_id == 1)
            {
                response_body.append(
                    reinterpret_cast<char const*>(rpayload.data()),
                    rpayload.size());
            }
            else if(rfh.type == http2::frame_type::goaway)
            {
                break;
            }
        }

        if(response_status == "200" && response_body == "hello")
            passed = true;
        else
        {
            detail = "status=" + response_status +
                     " body=" + response_body;
        }
    }
    catch(std::exception const& e)
    {
        detail = std::string("exception: ") + e.what();
    }

    server_thread.join();
    acceptor.close();
    report(2, "Basic request/response", passed, detail);
}

//------------------------------------------------------------------------------
//
// Test 3: PING (server-initiated)
//
//------------------------------------------------------------------------------
static void
test_ping(unsigned short port)
{
    net::io_context ioc;

    tcp::acceptor acceptor(ioc,
        tcp::endpoint(net::ip::make_address("127.0.0.1"), port));
    acceptor.set_option(net::socket_base::reuse_address(true));

    std::atomic<bool> ping_ack_ok{false};

    std::thread server_thread([&]()
    {
        beast::error_code ec;
        tcp::socket sock(ioc);
        acceptor.accept(sock, ec);
        if(ec) return;

        http2::settings srv_settings;
        srv_settings.max_concurrent_streams = 100;
        srv_settings.enable_push = false;

        if(!mock_server_handshake(sock, srv_settings, ec))
            return;

        // Send PING with payload {0x01, 0x02, ..., 0x08}
        std::uint8_t ping_data[8] = {0x01, 0x02, 0x03, 0x04,
                                     0x05, 0x06, 0x07, 0x08};
        send_ping(sock, ping_data, ec);
        if(ec)
            return;

        // Read frames until we get PING ACK or error
        for(int i = 0; i < 10; ++i)
        {
            http2::frame_header fh;
            std::vector<std::uint8_t> payload;
            if(!read_frame(sock, fh, payload, ec))
                break;

            if(fh.type == http2::frame_type::ping &&
               (fh.flags & http2::frame_flag::ack) &&
               fh.length == 8)
            {
                if(std::memcmp(payload.data(), ping_data, 8) == 0)
                    ping_ack_ok = true;
                break;
            }
            // Skip WINDOW_UPDATE or other frames the client may send
        }

        // Close gracefully
        send_goaway(sock, 0, H2_NO_ERROR, ec);

        beast::error_code ignore;
        sock.shutdown(tcp::socket::shutdown_both, ignore);
        sock.close(ignore);
    });

    // Client side: handshake, then run the async read pump
    bool passed = false;
    std::string detail;
    try
    {
        net::io_context client_ioc;
        tcp::socket client_sock(client_ioc);
        client_sock.connect(
            tcp::endpoint(net::ip::make_address("127.0.0.1"), port));

        http2::connection<tcp::socket&> conn(client_sock);
        beast::error_code ec;
        conn.handshake(ec);
        if(ec)
        {
            detail = "handshake failed: " + ec.message();
            server_thread.join();
            acceptor.close();
            report(3, "PING (server-initiated)", false, detail);
            return;
        }

        // async_run() dispatches frames; dispatch_frame() auto-sends PING ACK
        conn.async_run([](beast::error_code) {});
        client_ioc.run();

        // Verification is server-side
        server_thread.join();
        passed = ping_ack_ok.load();
        if(!passed)
            detail = "server did not receive matching PING ACK";
    }
    catch(std::exception const& e)
    {
        detail = std::string("exception: ") + e.what();
        server_thread.join();
    }

    acceptor.close();
    report(3, "PING (server-initiated)", passed, detail);
}

//------------------------------------------------------------------------------
//
// Test 4: GOAWAY
//
//------------------------------------------------------------------------------
static void
test_goaway(unsigned short port)
{
    net::io_context ioc;

    tcp::acceptor acceptor(ioc,
        tcp::endpoint(net::ip::make_address("127.0.0.1"), port));
    acceptor.set_option(net::socket_base::reuse_address(true));

    std::thread server_thread([&]()
    {
        beast::error_code ec;
        tcp::socket sock(ioc);
        acceptor.accept(sock, ec);
        if(ec) return;

        http2::settings srv_settings;
        srv_settings.max_concurrent_streams = 100;
        srv_settings.enable_push = false;

        if(!mock_server_handshake(sock, srv_settings, ec))
            return;

        // Send GOAWAY(last_stream_id=0, NO_ERROR) and close
        send_goaway(sock, 0, H2_NO_ERROR, ec);

        beast::error_code ignore;
        sock.shutdown(tcp::socket::shutdown_both, ignore);
        sock.close(ignore);
    });

    // Client side
    bool passed = false;
    std::string detail;
    try
    {
        net::io_context client_ioc;
        tcp::socket client_sock(client_ioc);
        client_sock.connect(
            tcp::endpoint(net::ip::make_address("127.0.0.1"), port));

        http2::connection<tcp::socket&> conn(client_sock);
        beast::error_code ec;
        conn.handshake(ec);
        if(ec)
        {
            detail = "handshake failed: " + ec.message();
            server_thread.join();
            acceptor.close();
            report(4, "GOAWAY", false, detail);
            return;
        }

        // async_run will process the GOAWAY then read EOF
        beast::error_code run_ec;
        conn.async_run([&run_ec](beast::error_code ec)
        {
            run_ec = ec;
        });
        client_ioc.run();

        // After GOAWAY + EOF, connection should not be open
        if(!conn.is_open())
            passed = true;
        else
            detail = "connection still open after GOAWAY";
    }
    catch(std::exception const& e)
    {
        detail = std::string("exception: ") + e.what();
    }

    server_thread.join();
    acceptor.close();
    report(4, "GOAWAY", passed, detail);
}

//------------------------------------------------------------------------------
//
// Test 5: RST_STREAM
//
//------------------------------------------------------------------------------
static void
test_rst_stream(unsigned short port)
{
    net::io_context ioc;

    tcp::acceptor acceptor(ioc,
        tcp::endpoint(net::ip::make_address("127.0.0.1"), port));
    acceptor.set_option(net::socket_base::reuse_address(true));

    std::thread server_thread([&]()
    {
        beast::error_code ec;
        tcp::socket sock(ioc);
        acceptor.accept(sock, ec);
        if(ec) return;

        http2::settings srv_settings;
        srv_settings.max_concurrent_streams = 100;
        srv_settings.enable_push = false;

        if(!mock_server_handshake(sock, srv_settings, ec))
            return;

        // Read client HEADERS on stream 1
        http2::frame_header fh;
        std::vector<std::uint8_t> payload;
        if(!read_frame(sock, fh, payload, ec))
            return;

        // Send RST_STREAM(stream_id=1, CANCEL)
        send_rst_stream(sock, 1, H2_CANCEL, ec);
        if(ec)
            return;

        // Send GOAWAY(NO_ERROR) and close
        send_goaway(sock, 1, H2_NO_ERROR, ec);

        beast::error_code ignore;
        sock.shutdown(tcp::socket::shutdown_both, ignore);
        sock.close(ignore);
    });

    // Client side
    bool passed = false;
    std::string detail;
    try
    {
        net::io_context client_ioc;
        tcp::socket client_sock(client_ioc);
        client_sock.connect(
            tcp::endpoint(net::ip::make_address("127.0.0.1"), port));

        http2::connection<tcp::socket&> conn(client_sock);
        beast::error_code ec;
        conn.handshake(ec);
        if(ec)
        {
            detail = "handshake failed: " + ec.message();
            server_thread.join();
            acceptor.close();
            report(5, "RST_STREAM", false, detail);
            return;
        }

        // Send HEADERS on stream 1 (raw)
        hpack::encoder client_enc;
        std::uint8_t hdr_buf[4096];
        std::size_t hdr_len = 0;

        hdr_len += client_enc.encode_field(
            hdr_buf + hdr_len, sizeof(hdr_buf) - hdr_len,
            ":method", "GET");
        hdr_len += client_enc.encode_field(
            hdr_buf + hdr_len, sizeof(hdr_buf) - hdr_len,
            ":path", "/");
        hdr_len += client_enc.encode_field(
            hdr_buf + hdr_len, sizeof(hdr_buf) - hdr_len,
            ":scheme", "http");
        hdr_len += client_enc.encode_field(
            hdr_buf + hdr_len, sizeof(hdr_buf) - hdr_len,
            ":authority", "127.0.0.1");

        http2::frame_header req_fh;
        req_fh.length = static_cast<std::uint32_t>(hdr_len);
        req_fh.type = http2::frame_type::headers;
        req_fh.flags = http2::frame_flag::end_headers |
                       http2::frame_flag::end_stream;
        req_fh.stream_id = 1;

        std::uint8_t frame_buf[http2::frame_header_size];
        http2::serialize_frame_header(frame_buf, req_fh);
        net::write(client_sock, net::buffer(frame_buf, http2::frame_header_size), ec);
        if(!ec)
            net::write(client_sock, net::buffer(hdr_buf, hdr_len), ec);
        if(ec)
        {
            detail = "send headers failed: " + ec.message();
            server_thread.join();
            acceptor.close();
            report(5, "RST_STREAM", false, detail);
            return;
        }

        // async_run will dispatch RST_STREAM (removes stream) + GOAWAY + EOF
        beast::error_code run_ec;
        conn.async_run([&run_ec](beast::error_code ec)
        {
            run_ec = ec;
        });
        client_ioc.run();

        // RST_STREAM should be processed cleanly (no protocol_error)
        if(run_ec == net::error::eof || !run_ec ||
           run_ec == http2::error::goaway_received)
            passed = true;
        else
            detail = "async_run error: " + run_ec.message();
    }
    catch(std::exception const& e)
    {
        detail = std::string("exception: ") + e.what();
    }

    server_thread.join();
    acceptor.close();
    report(5, "RST_STREAM", passed, detail);
}

//------------------------------------------------------------------------------
//
// Test 6: Frame size violation
//
//------------------------------------------------------------------------------
static void
test_frame_size_violation(unsigned short port)
{
    net::io_context ioc;

    tcp::acceptor acceptor(ioc,
        tcp::endpoint(net::ip::make_address("127.0.0.1"), port));
    acceptor.set_option(net::socket_base::reuse_address(true));

    std::thread server_thread([&]()
    {
        beast::error_code ec;
        tcp::socket sock(ioc);
        acceptor.accept(sock, ec);
        if(ec) return;

        http2::settings srv_settings;
        srv_settings.max_concurrent_streams = 100;
        srv_settings.enable_push = false;

        if(!mock_server_handshake(sock, srv_settings, ec))
            return;

        // Send a frame with length=16385 (exceeds default max_frame_size=16384)
        // Use DATA frame type on stream 0 (invalid, but the size check
        // should trigger first if implemented)
        std::uint32_t oversize = 16385;
        std::vector<std::uint8_t> big_payload(oversize, 0x42);

        // Send as a DATA frame on stream 1 (technically need an open stream,
        // but we're testing frame size validation which should happen first)
        send_frame(sock, http2::frame_type::data, 0, 1,
            big_payload.data(), oversize, ec);
        if(ec)
            return;

        // Close immediately so client gets EOF
        beast::error_code ignore;
        sock.shutdown(tcp::socket::shutdown_both, ignore);
        sock.close(ignore);
    });

    // Client side
    bool passed = false;
    std::string detail;
    try
    {
        net::io_context client_ioc;
        tcp::socket client_sock(client_ioc);
        client_sock.connect(
            tcp::endpoint(net::ip::make_address("127.0.0.1"), port));

        http2::connection<tcp::socket&> conn(client_sock);
        beast::error_code ec;
        conn.handshake(ec);
        if(ec)
        {
            detail = "handshake failed: " + ec.message();
            server_thread.join();
            acceptor.close();
            report(6, "Frame size violation", false, detail);
            return;
        }

        beast::error_code run_ec;
        conn.async_run([&run_ec](beast::error_code ec)
        {
            run_ec = ec;
        });
        client_ioc.run();

        // Currently async_run() does NOT validate frame size
        // (it reads whatever the frame header says).
        // This is a known conformance gap. The test documents it.
        // A conformant implementation would return frame_size_error.
        if(run_ec == http2::error::frame_size_error ||
           run_ec == http2::error::frame_too_large)
        {
            // If ever fixed, this would be the correct behavior
            passed = true;
        }
        else
        {
            // Known gap: no frame size check in async_run
            passed = false;
            detail = "known gap: no frame size check in async_run";
        }
    }
    catch(std::exception const& e)
    {
        detail = std::string("exception: ") + e.what();
    }

    server_thread.join();
    acceptor.close();
    report(6, "Frame size violation", passed, detail);
}

//------------------------------------------------------------------------------
//
// Test 7: Invalid SETTINGS (initial_window_size overflow)
//
//------------------------------------------------------------------------------
static void
test_invalid_settings(unsigned short port)
{
    net::io_context ioc;

    tcp::acceptor acceptor(ioc,
        tcp::endpoint(net::ip::make_address("127.0.0.1"), port));
    acceptor.set_option(net::socket_base::reuse_address(true));

    std::thread server_thread([&]()
    {
        beast::error_code ec;
        tcp::socket sock(ioc);
        acceptor.accept(sock, ec);
        if(ec) return;

        // Read client connection preface magic (24 bytes)
        std::uint8_t preface_buf[client_preface_size];
        read_exact(sock, preface_buf, client_preface_size, ec);
        if(ec) return;

        // Read client SETTINGS frame
        std::uint8_t frame_hdr_buf[http2::frame_header_size];
        read_exact(sock, frame_hdr_buf, http2::frame_header_size, ec);
        if(ec) return;

        http2::frame_header fh;
        http2::parse_frame_header(fh, frame_hdr_buf, ec);
        if(ec) return;

        if(fh.length > 0)
        {
            std::vector<std::uint8_t> payload(fh.length);
            read_exact(sock, payload.data(), fh.length, ec);
            if(ec) return;
        }

        // Send SETTINGS with initial_window_size = 0x80000000 (exceeds max)
        // Setting ID 0x04 = initial_window_size
        std::uint8_t bad_settings[6];
        bad_settings[0] = 0x00; // id high byte
        bad_settings[1] = 0x04; // id = INITIAL_WINDOW_SIZE
        bad_settings[2] = 0x80; // value = 0x80000000 (exceeds 2^31-1)
        bad_settings[3] = 0x00;
        bad_settings[4] = 0x00;
        bad_settings[5] = 0x00;

        send_frame(sock, http2::frame_type::settings, 0, 0,
            bad_settings, 6, ec);
        if(ec) return;

        // Give client a moment to read, then close
        beast::error_code ignore;
        sock.shutdown(tcp::socket::shutdown_both, ignore);
        sock.close(ignore);
    });

    // Client side
    bool passed = false;
    std::string detail;
    try
    {
        net::io_context client_ioc;
        tcp::socket client_sock(client_ioc);
        client_sock.connect(
            tcp::endpoint(net::ip::make_address("127.0.0.1"), port));

        http2::connection<tcp::socket&> conn(client_sock);
        beast::error_code ec;
        conn.handshake(ec);

        // handshake -> read_settings() -> parse_settings()
        // should return flow_control_error
        if(ec == http2::error::flow_control_error)
        {
            passed = true;
        }
        else if(ec)
        {
            // Some other error during handshake - might still be acceptable
            // if the connection detected the invalid setting
            detail = "got error: " + ec.message() +
                     " (expected flow_control_error)";
        }
        else
        {
            detail = "handshake succeeded despite invalid settings";
        }
    }
    catch(std::exception const& e)
    {
        detail = std::string("exception: ") + e.what();
    }

    server_thread.join();
    acceptor.close();
    report(7, "Invalid SETTINGS", passed, detail);
}

//------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    unsigned short port = 9081;

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
                "Usage: h2spec_client [-p port]\n"
                "  -p, --port PORT   Port for mock server (default: 9081)\n"
                "  -h, --help        Show this help\n";
            return 0;
        }
    }

    std::cout << "h2spec_client - HTTP/2 client conformance tests\n"
              << "================================================\n";

    // Run each test on a different port to avoid bind conflicts
    // (each test creates its own acceptor)
    test_connection_preface(port);
    test_basic_request_response(port + 1);
    test_ping(port + 2);
    test_goaway(port + 3);
    test_rst_stream(port + 4);
    test_frame_size_violation(port + 5);
    test_invalid_settings(port + 6);

    std::cout << "================================================\n";

    int pass_count = 0;
    int fail_count = 0;
    for(auto const& r : results)
    {
        if(r.passed)
            ++pass_count;
        else
            ++fail_count;
    }

    std::cout << "Results: " << pass_count << " passed, "
              << fail_count << " failed\n";

    return fail_count > 0 ? 1 : 0;
}
