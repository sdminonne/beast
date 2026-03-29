//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http2/hpack/decoder.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <vector>

namespace boost {
namespace beast {
namespace http2 {
namespace hpack {

class decoder_test : public unit_test::suite
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
    testIndexed()
    {
        // Decode an indexed header (static table entry 2 = :method GET)
        decoder dec;
        header_collector handler;
        error_code ec;

        // 0x82 = indexed, index=2
        std::uint8_t data[] = {0x82};
        dec.decode(data, sizeof(data), handler, ec);

        BEAST_EXPECT(! ec);
        BEAST_EXPECT(handler.headers.size() == 1);
        BEAST_EXPECT(handler.headers[0].name == ":method");
        BEAST_EXPECT(handler.headers[0].value == "GET");
    }

    void
    testLiteralWithIndexing()
    {
        // RFC 7541 Section C.2.1: Literal Header Field with Indexing
        // custom-key: custom-header
        decoder dec;
        header_collector handler;
        error_code ec;

        std::uint8_t data[] = {
            0x40,                                           // literal with indexing, new name
            0x0a,                                           // name length = 10
            'c','u','s','t','o','m','-','k','e','y',        // name
            0x0d,                                           // value length = 13
            'c','u','s','t','o','m','-','h','e','a','d','e','r' // value
        };

        dec.decode(data, sizeof(data), handler, ec);

        BEAST_EXPECT(! ec);
        BEAST_EXPECT(handler.headers.size() == 1);
        BEAST_EXPECT(handler.headers[0].name == "custom-key");
        BEAST_EXPECT(handler.headers[0].value == "custom-header");

        // Verify it was added to the dynamic table
        BEAST_EXPECT(dec.table().size() == 1);
    }

    void
    testLiteralWithoutIndexing()
    {
        // Literal without indexing, indexed name (index 4 = :path)
        decoder dec;
        header_collector handler;
        error_code ec;

        std::uint8_t data[] = {
            0x04,                   // literal without indexing, index=4 (:path)
            0x0c,                   // value length = 12
            '/','s','a','m','p','l','e','/','p','a','t','h'
        };

        dec.decode(data, sizeof(data), handler, ec);

        BEAST_EXPECT(! ec);
        BEAST_EXPECT(handler.headers.size() == 1);
        BEAST_EXPECT(handler.headers[0].name == ":path");
        BEAST_EXPECT(handler.headers[0].value == "/sample/path");

        // Should NOT be in the dynamic table
        BEAST_EXPECT(dec.table().size() == 0);
    }

    void
    testLiteralNeverIndexed()
    {
        // Literal never indexed, indexed name
        decoder dec;
        header_collector handler;
        error_code ec;

        std::uint8_t data[] = {
            0x14,               // never indexed, index=4 (:path)
            0x0c,               // value length = 12
            '/','s','a','m','p','l','e','/','p','a','t','h'
        };

        dec.decode(data, sizeof(data), handler, ec);

        BEAST_EXPECT(! ec);
        BEAST_EXPECT(handler.headers.size() == 1);
        BEAST_EXPECT(handler.headers[0].name == ":path");
        BEAST_EXPECT(handler.headers[0].value == "/sample/path");
        BEAST_EXPECT(handler.headers[0].sensitive == true);

        // Should NOT be in the dynamic table
        BEAST_EXPECT(dec.table().size() == 0);
    }

    void
    testMultipleHeaders()
    {
        // Decode multiple indexed headers
        decoder dec;
        header_collector handler;
        error_code ec;

        std::uint8_t data[] = {
            0x82, // :method GET
            0x86, // :scheme http
            0x84, // :path /
        };

        dec.decode(data, sizeof(data), handler, ec);

        BEAST_EXPECT(! ec);
        BEAST_EXPECT(handler.headers.size() == 3);
        BEAST_EXPECT(handler.headers[0].name == ":method");
        BEAST_EXPECT(handler.headers[0].value == "GET");
        BEAST_EXPECT(handler.headers[1].name == ":scheme");
        BEAST_EXPECT(handler.headers[1].value == "http");
        BEAST_EXPECT(handler.headers[2].name == ":path");
        BEAST_EXPECT(handler.headers[2].value == "/");
    }

    void
    testTableSizeUpdate()
    {
        decoder dec;
        header_collector handler;
        error_code ec;

        // Dynamic table size update to 0
        std::uint8_t data[] = {0x20}; // 001 + 0 = set size to 0
        dec.decode(data, sizeof(data), handler, ec);

        BEAST_EXPECT(! ec);
        BEAST_EXPECT(dec.table().max_size() == 0);
    }

    void
    run() override
    {
        testIndexed();
        testLiteralWithIndexing();
        testLiteralWithoutIndexing();
        testLiteralNeverIndexed();
        testMultipleHeaders();
        testTableSizeUpdate();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http2_hpack,decoder);

} // hpack
} // http2
} // beast
} // boost
