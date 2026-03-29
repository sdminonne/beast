//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http2/frame.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <cstring>

namespace boost {
namespace beast {
namespace http2 {

class frame_test : public unit_test::suite
{
public:
    void
    testConstants()
    {
        BEAST_EXPECT(frame_header_size == 9);
        BEAST_EXPECT(default_max_frame_size == 16384);
        BEAST_EXPECT(max_allowed_frame_size == 16777215);
        BEAST_EXPECT(default_initial_window_size == 65535);
        BEAST_EXPECT(max_window_size == 2147483647);
    }

    void
    testFrameTypes()
    {
        BEAST_EXPECT(static_cast<std::uint8_t>(frame_type::data) == 0x00);
        BEAST_EXPECT(static_cast<std::uint8_t>(frame_type::headers) == 0x01);
        BEAST_EXPECT(static_cast<std::uint8_t>(frame_type::priority) == 0x02);
        BEAST_EXPECT(static_cast<std::uint8_t>(frame_type::rst_stream) == 0x03);
        BEAST_EXPECT(static_cast<std::uint8_t>(frame_type::settings) == 0x04);
        BEAST_EXPECT(static_cast<std::uint8_t>(frame_type::push_promise) == 0x05);
        BEAST_EXPECT(static_cast<std::uint8_t>(frame_type::ping) == 0x06);
        BEAST_EXPECT(static_cast<std::uint8_t>(frame_type::goaway) == 0x07);
        BEAST_EXPECT(static_cast<std::uint8_t>(frame_type::window_update) == 0x08);
        BEAST_EXPECT(static_cast<std::uint8_t>(frame_type::continuation) == 0x09);
    }

    void
    testFrameFlags()
    {
        BEAST_EXPECT(frame_flag::end_stream == 0x01);
        BEAST_EXPECT(frame_flag::ack == 0x01);
        BEAST_EXPECT(frame_flag::end_headers == 0x04);
        BEAST_EXPECT(frame_flag::padded == 0x08);
        BEAST_EXPECT(frame_flag::priority == 0x20);
    }

    void
    testSerializeParse()
    {
        // Test basic round-trip
        {
            frame_header fh;
            fh.length = 100;
            fh.type = frame_type::data;
            fh.flags = frame_flag::end_stream;
            fh.stream_id = 1;

            std::uint8_t buf[frame_header_size];
            serialize_frame_header(buf, fh);

            frame_header fh2;
            error_code ec;
            parse_frame_header(fh2, buf, ec);

            BEAST_EXPECT(! ec);
            BEAST_EXPECT(fh2.length == 100);
            BEAST_EXPECT(fh2.type == frame_type::data);
            BEAST_EXPECT(fh2.flags == frame_flag::end_stream);
            BEAST_EXPECT(fh2.stream_id == 1);
        }

        // Test maximum values
        {
            frame_header fh;
            fh.length = max_allowed_frame_size;
            fh.type = frame_type::continuation;
            fh.flags = 0xff;
            fh.stream_id = 0x7fffffffu;

            std::uint8_t buf[frame_header_size];
            serialize_frame_header(buf, fh);

            frame_header fh2;
            error_code ec;
            parse_frame_header(fh2, buf, ec);

            BEAST_EXPECT(! ec);
            BEAST_EXPECT(fh2.length == max_allowed_frame_size);
            BEAST_EXPECT(fh2.type == frame_type::continuation);
            BEAST_EXPECT(fh2.flags == 0xff);
            BEAST_EXPECT(fh2.stream_id == 0x7fffffffu);
        }

        // Test zero values
        {
            frame_header fh;
            fh.length = 0;
            fh.type = frame_type::data;
            fh.flags = 0;
            fh.stream_id = 0;

            std::uint8_t buf[frame_header_size];
            serialize_frame_header(buf, fh);

            frame_header fh2;
            error_code ec;
            parse_frame_header(fh2, buf, ec);

            BEAST_EXPECT(! ec);
            BEAST_EXPECT(fh2.length == 0);
            BEAST_EXPECT(fh2.type == frame_type::data);
            BEAST_EXPECT(fh2.flags == 0);
            BEAST_EXPECT(fh2.stream_id == 0);
        }

        // Test SETTINGS frame (stream_id = 0)
        {
            frame_header fh;
            fh.length = 12;
            fh.type = frame_type::settings;
            fh.flags = 0;
            fh.stream_id = 0;

            std::uint8_t buf[frame_header_size];
            serialize_frame_header(buf, fh);

            frame_header fh2;
            error_code ec;
            parse_frame_header(fh2, buf, ec);

            BEAST_EXPECT(! ec);
            BEAST_EXPECT(fh2.length == 12);
            BEAST_EXPECT(fh2.type == frame_type::settings);
            BEAST_EXPECT(fh2.stream_id == 0);
        }

        // Test reserved bit is masked in stream_id
        {
            frame_header fh;
            fh.length = 0;
            fh.type = frame_type::data;
            fh.flags = 0;
            fh.stream_id = 0x80000001u; // R bit set

            std::uint8_t buf[frame_header_size];
            serialize_frame_header(buf, fh);

            frame_header fh2;
            error_code ec;
            parse_frame_header(fh2, buf, ec);

            BEAST_EXPECT(! ec);
            // R bit should be masked off
            BEAST_EXPECT(fh2.stream_id == 1);
        }

        // Test known wire bytes
        {
            // HEADERS frame: length=5, type=0x01, flags=0x04 (END_HEADERS),
            // stream_id=1
            std::uint8_t wire[] = {
                0x00, 0x00, 0x05, // length = 5
                0x01,             // type = HEADERS
                0x04,             // flags = END_HEADERS
                0x00, 0x00, 0x00, 0x01 // stream_id = 1
            };

            frame_header fh;
            error_code ec;
            parse_frame_header(fh, wire, ec);

            BEAST_EXPECT(! ec);
            BEAST_EXPECT(fh.length == 5);
            BEAST_EXPECT(fh.type == frame_type::headers);
            BEAST_EXPECT(fh.flags == frame_flag::end_headers);
            BEAST_EXPECT(fh.stream_id == 1);
        }
    }

    void
    run() override
    {
        testConstants();
        testFrameTypes();
        testFrameFlags();
        testSerializeParse();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http2,frame);

} // http2
} // beast
} // boost
