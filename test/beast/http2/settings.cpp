//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http2/settings.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <cstring>

namespace boost {
namespace beast {
namespace http2 {

class settings_test : public unit_test::suite
{
public:
    void
    testDefaults()
    {
        settings s;
        BEAST_EXPECT(s.header_table_size == 4096);
        BEAST_EXPECT(s.enable_push == true);
        BEAST_EXPECT(s.max_concurrent_streams == 100);
        BEAST_EXPECT(s.initial_window_size == 65535);
        BEAST_EXPECT(s.max_frame_size == 16384);
        BEAST_EXPECT(s.max_header_list_size == 8192);
    }

    void
    testSettingIds()
    {
        BEAST_EXPECT(static_cast<std::uint16_t>(setting_id::header_table_size) == 0x01);
        BEAST_EXPECT(static_cast<std::uint16_t>(setting_id::enable_push) == 0x02);
        BEAST_EXPECT(static_cast<std::uint16_t>(setting_id::max_concurrent_streams) == 0x03);
        BEAST_EXPECT(static_cast<std::uint16_t>(setting_id::initial_window_size) == 0x04);
        BEAST_EXPECT(static_cast<std::uint16_t>(setting_id::max_frame_size) == 0x05);
        BEAST_EXPECT(static_cast<std::uint16_t>(setting_id::max_header_list_size) == 0x06);
    }

    void
    testSerializeParams()
    {
        setting_parameter params[2];
        params[0].id = setting_id::header_table_size;
        params[0].value = 8192;
        params[1].id = setting_id::max_frame_size;
        params[1].value = 32768;

        std::uint8_t buf[12];
        auto n = serialize_settings(buf, params, 2);
        BEAST_EXPECT(n == 12);

        // Verify big-endian encoding
        // First param: id=0x0001, value=0x00002000
        BEAST_EXPECT(buf[0] == 0x00);
        BEAST_EXPECT(buf[1] == 0x01);
        BEAST_EXPECT(buf[2] == 0x00);
        BEAST_EXPECT(buf[3] == 0x00);
        BEAST_EXPECT(buf[4] == 0x20);
        BEAST_EXPECT(buf[5] == 0x00);

        // Second param: id=0x0005, value=0x00008000
        BEAST_EXPECT(buf[6] == 0x00);
        BEAST_EXPECT(buf[7] == 0x05);
        BEAST_EXPECT(buf[8] == 0x00);
        BEAST_EXPECT(buf[9] == 0x00);
        BEAST_EXPECT(buf[10] == 0x80);
        BEAST_EXPECT(buf[11] == 0x00);
    }

    void
    testSerializeSettings()
    {
        // Default settings should produce 0 bytes
        // (only non-default values are serialized)
        {
            settings s;
            std::uint8_t buf[36];
            auto n = serialize_settings(buf, s);
            BEAST_EXPECT(n == 0);
        }

        // Non-default settings
        {
            settings s;
            s.header_table_size = 8192;
            s.max_frame_size = 32768;

            std::uint8_t buf[36];
            auto n = serialize_settings(buf, s);
            BEAST_EXPECT(n == 12);
        }
    }

    void
    testParseSettings()
    {
        // Round-trip test
        {
            setting_parameter params[3];
            params[0].id = setting_id::header_table_size;
            params[0].value = 8192;
            params[1].id = setting_id::max_concurrent_streams;
            params[1].value = 200;
            params[2].id = setting_id::initial_window_size;
            params[2].value = 131072;

            std::uint8_t buf[18];
            serialize_settings(buf, params, 3);

            settings s;
            error_code ec;
            parse_settings(s, buf, 18, ec);

            BEAST_EXPECT(! ec);
            BEAST_EXPECT(s.header_table_size == 8192);
            BEAST_EXPECT(s.max_concurrent_streams == 200);
            BEAST_EXPECT(s.initial_window_size == 131072);
        }

        // Invalid length (not multiple of 6)
        {
            std::uint8_t buf[7] = {};
            settings s;
            error_code ec;
            parse_settings(s, buf, 7, ec);
            BEAST_EXPECT(ec == error::frame_size_error);
        }

        // Invalid enable_push value
        {
            setting_parameter params[1];
            params[0].id = setting_id::enable_push;
            params[0].value = 2; // must be 0 or 1

            std::uint8_t buf[6];
            serialize_settings(buf, params, 1);

            settings s;
            error_code ec;
            parse_settings(s, buf, 6, ec);
            BEAST_EXPECT(ec == error::protocol_error);
        }

        // Invalid initial_window_size
        {
            setting_parameter params[1];
            params[0].id = setting_id::initial_window_size;
            params[0].value = static_cast<std::uint32_t>(max_window_size) + 1;

            std::uint8_t buf[6];
            serialize_settings(buf, params, 1);

            settings s;
            error_code ec;
            parse_settings(s, buf, 6, ec);
            BEAST_EXPECT(ec == error::flow_control_error);
        }

        // Invalid max_frame_size (too small)
        {
            setting_parameter params[1];
            params[0].id = setting_id::max_frame_size;
            params[0].value = default_max_frame_size - 1;

            std::uint8_t buf[6];
            serialize_settings(buf, params, 1);

            settings s;
            error_code ec;
            parse_settings(s, buf, 6, ec);
            BEAST_EXPECT(ec == error::protocol_error);
        }

        // Invalid max_frame_size (too large)
        {
            setting_parameter params[1];
            params[0].id = setting_id::max_frame_size;
            params[0].value = max_allowed_frame_size + 1;

            std::uint8_t buf[6];
            serialize_settings(buf, params, 1);

            settings s;
            error_code ec;
            parse_settings(s, buf, 6, ec);
            BEAST_EXPECT(ec == error::protocol_error);
        }

        // Unknown settings are ignored
        {
            std::uint8_t buf[6] = {
                0x00, 0xff, // unknown id = 255
                0x00, 0x00, 0x00, 0x01  // value = 1
            };

            settings s;
            error_code ec;
            parse_settings(s, buf, 6, ec);
            BEAST_EXPECT(! ec);
        }

        // Empty settings payload is valid
        {
            settings s;
            error_code ec;
            parse_settings(s, nullptr, 0, ec);
            BEAST_EXPECT(! ec);
        }
    }

    void
    testValidateSettings()
    {
        // Valid defaults
        {
            settings s;
            error_code ec;
            validate_settings(s, ec);
            BEAST_EXPECT(! ec);
        }

        // Invalid initial_window_size
        {
            settings s;
            s.initial_window_size = static_cast<std::uint32_t>(max_window_size) + 1;
            error_code ec;
            validate_settings(s, ec);
            BEAST_EXPECT(ec == error::flow_control_error);
        }

        // Invalid max_frame_size (too small)
        {
            settings s;
            s.max_frame_size = default_max_frame_size - 1;
            error_code ec;
            validate_settings(s, ec);
            BEAST_EXPECT(ec == error::protocol_error);
        }

        // Invalid max_frame_size (too large)
        {
            settings s;
            s.max_frame_size = max_allowed_frame_size + 1;
            error_code ec;
            validate_settings(s, ec);
            BEAST_EXPECT(ec == error::protocol_error);
        }
    }

    void
    run() override
    {
        testDefaults();
        testSettingIds();
        testSerializeParams();
        testSerializeSettings();
        testParseSettings();
        testValidateSettings();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http2,settings);

} // http2
} // beast
} // boost
