//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http2/hpack/dynamic_table.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace http2 {
namespace hpack {

class dynamic_table_test : public unit_test::suite
{
public:
    void
    testInsertAndLookup()
    {
        dynamic_table dt;

        dt.insert("custom-key", "custom-value");
        BEAST_EXPECT(dt.size() == 1);
        BEAST_EXPECT(dt.current_size() == 10 + 12 + 32);

        auto entry = dt.at(0);
        BEAST_EXPECT(entry.name == "custom-key");
        BEAST_EXPECT(entry.value == "custom-value");
    }

    void
    testEviction()
    {
        dynamic_table dt;
        dt.set_max_size(100);

        // Each entry: name(1) + value(1) + 32 = 34 bytes
        dt.insert("a", "1");
        dt.insert("b", "2");
        BEAST_EXPECT(dt.size() == 2);
        BEAST_EXPECT(dt.current_size() == 68);

        // This should evict the oldest
        dt.insert("c", "3");
        BEAST_EXPECT(dt.size() == 2);

        // Most recent is at index 0
        auto entry0 = dt.at(0);
        BEAST_EXPECT(entry0.name == "c");

        auto entry1 = dt.at(1);
        BEAST_EXPECT(entry1.name == "b");
    }

    void
    testSetMaxSize()
    {
        dynamic_table dt;
        dt.insert("custom-key", "custom-value");
        BEAST_EXPECT(dt.size() == 1);

        // Setting max size to 0 clears the table
        dt.set_max_size(0);
        BEAST_EXPECT(dt.size() == 0);
        BEAST_EXPECT(dt.current_size() == 0);
    }

    void
    testFind()
    {
        dynamic_table dt;
        dt.insert("custom-key", "custom-value");
        dt.insert("another-key", "another-value");

        // Exact match
        {
            bool name_match = false;
            auto idx = dt.find("custom-key", "custom-value", name_match);
            BEAST_EXPECT(idx == 1); // older entry
            BEAST_EXPECT(! name_match);
        }

        // Name match only
        {
            bool name_match = false;
            auto idx = dt.find("custom-key", "other-value", name_match);
            BEAST_EXPECT(idx == 1);
            BEAST_EXPECT(name_match);
        }

        // No match
        {
            bool name_match = false;
            auto idx = dt.find("missing", "value", name_match);
            BEAST_EXPECT(idx == static_cast<std::size_t>(-1));
            BEAST_EXPECT(! name_match);
        }
    }

    void
    testClear()
    {
        dynamic_table dt;
        dt.insert("a", "b");
        dt.insert("c", "d");
        BEAST_EXPECT(dt.size() == 2);

        dt.clear();
        BEAST_EXPECT(dt.size() == 0);
        BEAST_EXPECT(dt.current_size() == 0);
    }

    void
    testOversizedEntry()
    {
        dynamic_table dt;
        dt.set_max_size(32); // Too small for any entry with overhead

        // Entry size = 1 + 1 + 32 = 34 > max_size=32
        // Should clear the table and not insert
        dt.insert("a", "b");
        BEAST_EXPECT(dt.size() == 0);
    }

    void
    run() override
    {
        testInsertAndLookup();
        testEviction();
        testSetMaxSize();
        testFind();
        testClear();
        testOversizedEntry();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http2_hpack,dynamic_table);

} // hpack
} // http2
} // beast
} // boost
