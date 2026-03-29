//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http2/hpack/integer.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace http2 {
namespace hpack {

class integer_test : public unit_test::suite
{
public:
    void
    testEncodeDecode()
    {
        // RFC 7541 Section C.1.1: Encoding 10 with 5-bit prefix
        {
            std::uint8_t buf[16];
            auto n = encode_integer(buf, sizeof(buf), 10, 5, 0x00);
            BEAST_EXPECT(n == 1);
            BEAST_EXPECT((buf[0] & 0x1f) == 10);

            std::uint64_t value;
            std::size_t consumed;
            error_code ec;
            decode_integer(buf, n, value, 5, consumed, ec);
            BEAST_EXPECT(! ec);
            BEAST_EXPECT(value == 10);
            BEAST_EXPECT(consumed == 1);
        }

        // RFC 7541 Section C.1.2: Encoding 1337 with 5-bit prefix
        {
            std::uint8_t buf[16];
            auto n = encode_integer(buf, sizeof(buf), 1337, 5, 0x00);
            BEAST_EXPECT(n == 3);
            BEAST_EXPECT((buf[0] & 0x1f) == 31);
            BEAST_EXPECT(buf[1] == 0x9a);
            BEAST_EXPECT(buf[2] == 0x0a);

            std::uint64_t value;
            std::size_t consumed;
            error_code ec;
            decode_integer(buf, n, value, 5, consumed, ec);
            BEAST_EXPECT(! ec);
            BEAST_EXPECT(value == 1337);
            BEAST_EXPECT(consumed == 3);
        }

        // RFC 7541 Section C.1.3: Encoding 42 with 8-bit prefix
        {
            std::uint8_t buf[16];
            auto n = encode_integer(buf, sizeof(buf), 42, 8, 0x00);
            BEAST_EXPECT(n == 1);
            BEAST_EXPECT(buf[0] == 42);

            std::uint64_t value;
            std::size_t consumed;
            error_code ec;
            decode_integer(buf, n, value, 8, consumed, ec);
            BEAST_EXPECT(! ec);
            BEAST_EXPECT(value == 42);
            BEAST_EXPECT(consumed == 1);
        }

        // Test with pattern bits
        {
            std::uint8_t buf[16];
            auto n = encode_integer(buf, sizeof(buf), 10, 5, 0xe0);
            BEAST_EXPECT(n == 1);
            BEAST_EXPECT(buf[0] == 0xea); // 0xe0 | 10

            std::uint64_t value;
            std::size_t consumed;
            error_code ec;
            decode_integer(buf, n, value, 5, consumed, ec);
            BEAST_EXPECT(! ec);
            BEAST_EXPECT(value == 10);
        }

        // Test zero value
        {
            std::uint8_t buf[16];
            auto n = encode_integer(buf, sizeof(buf), 0, 5, 0x00);
            BEAST_EXPECT(n == 1);
            BEAST_EXPECT((buf[0] & 0x1f) == 0);

            std::uint64_t value;
            std::size_t consumed;
            error_code ec;
            decode_integer(buf, n, value, 5, consumed, ec);
            BEAST_EXPECT(! ec);
            BEAST_EXPECT(value == 0);
        }

        // Test large value
        {
            std::uint8_t buf[16];
            auto n = encode_integer(buf, sizeof(buf), 100000, 5, 0x00);
            BEAST_EXPECT(n > 1);

            std::uint64_t value;
            std::size_t consumed;
            error_code ec;
            decode_integer(buf, n, value, 5, consumed, ec);
            BEAST_EXPECT(! ec);
            BEAST_EXPECT(value == 100000);
        }
    }

    void
    testErrors()
    {
        // Empty input
        {
            std::uint64_t value;
            std::size_t consumed;
            error_code ec;
            decode_integer(nullptr, 0, value, 5, consumed, ec);
            BEAST_EXPECT(ec == error::need_more);
        }

        // Truncated multi-byte
        {
            std::uint8_t buf[] = {0x1f}; // prefix full, needs more
            std::uint64_t value;
            std::size_t consumed;
            error_code ec;
            decode_integer(buf, 1, value, 5, consumed, ec);
            BEAST_EXPECT(ec == error::need_more);
        }
    }

    void
    run() override
    {
        testEncodeDecode();
        testErrors();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http2_hpack,integer);

} // hpack
} // http2
} // beast
} // boost
