//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http2/hpack/static_table.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace http2 {
namespace hpack {

class static_table_test : public unit_test::suite
{
public:
    void
    testEntries()
    {
        // RFC 7541 Appendix A
        BEAST_EXPECT(static_table_entry(1).name == ":authority");
        BEAST_EXPECT(static_table_entry(1).value == "");

        BEAST_EXPECT(static_table_entry(2).name == ":method");
        BEAST_EXPECT(static_table_entry(2).value == "GET");

        BEAST_EXPECT(static_table_entry(3).name == ":method");
        BEAST_EXPECT(static_table_entry(3).value == "POST");

        BEAST_EXPECT(static_table_entry(4).name == ":path");
        BEAST_EXPECT(static_table_entry(4).value == "/");

        BEAST_EXPECT(static_table_entry(5).name == ":path");
        BEAST_EXPECT(static_table_entry(5).value == "/index.html");

        BEAST_EXPECT(static_table_entry(6).name == ":scheme");
        BEAST_EXPECT(static_table_entry(6).value == "http");

        BEAST_EXPECT(static_table_entry(7).name == ":scheme");
        BEAST_EXPECT(static_table_entry(7).value == "https");

        BEAST_EXPECT(static_table_entry(8).name == ":status");
        BEAST_EXPECT(static_table_entry(8).value == "200");

        BEAST_EXPECT(static_table_entry(16).name == "accept-encoding");
        BEAST_EXPECT(static_table_entry(16).value == "gzip, deflate");

        BEAST_EXPECT(static_table_entry(61).name == "www-authenticate");
        BEAST_EXPECT(static_table_entry(61).value == "");
    }

    void
    testFind()
    {
        // Exact match
        {
            bool name_match = false;
            auto idx = static_table_find(":method", "GET", name_match);
            BEAST_EXPECT(idx == 2);
            BEAST_EXPECT(! name_match);
        }

        // Name-only match
        {
            bool name_match = false;
            auto idx = static_table_find(":method", "PUT", name_match);
            BEAST_EXPECT(idx == 2); // first :method entry
            BEAST_EXPECT(name_match);
        }

        // No match
        {
            bool name_match = false;
            auto idx = static_table_find("x-custom", "value", name_match);
            BEAST_EXPECT(idx == 0);
            BEAST_EXPECT(! name_match);
        }

        // Exact match with value
        {
            bool name_match = false;
            auto idx = static_table_find(":status", "200", name_match);
            BEAST_EXPECT(idx == 8);
        }
    }

    void
    run() override
    {
        testEntries();
        testFind();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http2_hpack,static_table);

} // hpack
} // http2
} // beast
} // boost
