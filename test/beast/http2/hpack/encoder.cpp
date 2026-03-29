//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http2/hpack/encoder.hpp>

#include <boost/beast/http2/hpack/decoder.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <vector>

namespace boost {
namespace beast {
namespace http2 {
namespace hpack {

class encoder_test : public unit_test::suite
{
    // Helper to collect decoded headers
    struct header_collector : decode_handler
    {
        struct header
        {
            std::string name;
            std::string value;
            bool sensitive;
        };
        std::vector<header> headers;

        void on_header(
            string_view name,
            string_view value,
            bool sensitive,
            error_code&) override
        {
            headers.push_back({
                std::string(name.data(), name.size()),
                std::string(value.data(), value.size()),
                sensitive});
        }
    };

public:
    void
    testEncodeIndexed()
    {
        // Encoding :method GET should produce indexed representation
        encoder enc;
        std::uint8_t buf[256];
        auto n = enc.encode_field(buf, sizeof(buf),
            ":method", "GET");
        BEAST_EXPECT(n > 0);
        BEAST_EXPECT(buf[0] == 0x82); // indexed, index=2
    }

    void
    testEncodeLiteral()
    {
        // Encoding a custom header should produce literal with indexing
        encoder enc;
        std::uint8_t buf[256];
        auto n = enc.encode_field(buf, sizeof(buf),
            "custom-key", "custom-value", true);
        BEAST_EXPECT(n > 0);
        // First byte should indicate literal with indexing, new name
        BEAST_EXPECT((buf[0] & 0xc0) == 0x40);

        // Should be added to dynamic table
        BEAST_EXPECT(enc.table().size() == 1);
    }

    void
    testEncodeNeverIndexed()
    {
        encoder enc;
        std::uint8_t buf[256];
        auto n = enc.encode_field(buf, sizeof(buf),
            "password", "secret", false, true);
        BEAST_EXPECT(n > 0);
        // First byte should indicate never indexed
        BEAST_EXPECT((buf[0] & 0xf0) == 0x10);
    }

    void
    testRoundTrip()
    {
        // Encode with encoder, decode with decoder, verify match
        encoder enc;
        decoder dec;

        std::uint8_t buf[1024];
        std::size_t total = 0;

        // Encode several headers
        auto n = enc.encode_field(buf + total, sizeof(buf) - total,
            ":method", "GET");
        BEAST_EXPECT(n > 0);
        total += n;

        n = enc.encode_field(buf + total, sizeof(buf) - total,
            ":path", "/index.html");
        BEAST_EXPECT(n > 0);
        total += n;

        n = enc.encode_field(buf + total, sizeof(buf) - total,
            ":scheme", "https");
        BEAST_EXPECT(n > 0);
        total += n;

        n = enc.encode_field(buf + total, sizeof(buf) - total,
            "custom-key", "custom-value");
        BEAST_EXPECT(n > 0);
        total += n;

        // Decode
        header_collector handler;
        error_code ec;
        dec.decode(buf, total, handler, ec);

        BEAST_EXPECT(! ec);
        BEAST_EXPECT(handler.headers.size() == 4);
        BEAST_EXPECT(handler.headers[0].name == ":method");
        BEAST_EXPECT(handler.headers[0].value == "GET");
        BEAST_EXPECT(handler.headers[1].name == ":path");
        BEAST_EXPECT(handler.headers[1].value == "/index.html");
        BEAST_EXPECT(handler.headers[2].name == ":scheme");
        BEAST_EXPECT(handler.headers[2].value == "https");
        BEAST_EXPECT(handler.headers[3].name == "custom-key");
        BEAST_EXPECT(handler.headers[3].value == "custom-value");
    }

    void
    testTableSizeUpdate()
    {
        encoder enc;
        std::uint8_t buf[256];
        auto n = enc.encode_table_size_update(buf, sizeof(buf), 0);
        BEAST_EXPECT(n > 0);
        BEAST_EXPECT(buf[0] == 0x20); // 001 + 0
    }

    void
    run() override
    {
        testEncodeIndexed();
        testEncodeLiteral();
        testEncodeNeverIndexed();
        testRoundTrip();
        testTableSizeUpdate();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http2_hpack,encoder);

} // hpack
} // http2
} // beast
} // boost
