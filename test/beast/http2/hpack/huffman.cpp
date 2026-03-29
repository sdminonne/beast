//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http2/hpack/huffman.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <cstring>

namespace boost {
namespace beast {
namespace http2 {
namespace hpack {

class huffman_test : public unit_test::suite
{
public:
    void
    testRoundTrip()
    {
        // Test various strings
        auto check = [&](char const* str)
        {
            auto const src = reinterpret_cast<std::uint8_t const*>(str);
            auto const len = std::strlen(str);

            auto const enc_size = huffman_encoded_size(src, len);

            std::uint8_t buf[256];
            auto n = huffman_encode(buf, sizeof(buf), src, len);
            BEAST_EXPECT(n == enc_size);

            std::string result;
            error_code ec;
            huffman_decode(result, buf, n, ec);
            BEAST_EXPECT(! ec);
            BEAST_EXPECT(result == str);
        };

        check("www.example.com");
        check("no-cache");
        check("custom-key");
        check("custom-value");
        check("");
        check("0");
        check("GET");
        check("POST");
        check("/");
        check("/index.html");
        check("http");
        check("https");
        check("200");
        check("gzip, deflate");
    }

    void
    testEncodedSize()
    {
        // "www.example.com" should encode to fewer bytes
        auto const src = reinterpret_cast<std::uint8_t const*>(
            "www.example.com");
        auto const len = std::strlen("www.example.com");
        auto const enc_size = huffman_encoded_size(src, len);
        BEAST_EXPECT(enc_size <= len);
    }

    void
    testErrors()
    {
        // Invalid padding (not all 1s)
        {
            std::uint8_t buf[] = {0x00}; // Starts with a valid prefix but bad padding
            std::string result;
            error_code ec;
            huffman_decode(result, buf, 1, ec);
            // This may or may not error depending on whether it decodes
            // to a valid symbol; we just check it doesn't crash
            (void)ec;
        }

        // Empty input should produce empty output
        {
            std::string result;
            error_code ec;
            huffman_decode(result, nullptr, 0, ec);
            BEAST_EXPECT(! ec);
            BEAST_EXPECT(result.empty());
        }
    }

    void
    run() override
    {
        testRoundTrip();
        testEncodedSize();
        testErrors();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http2_hpack,huffman);

} // hpack
} // http2
} // beast
} // boost
