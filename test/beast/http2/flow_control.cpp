//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http2/detail/flow_control.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace http2 {

class flow_control_test : public unit_test::suite
{
public:
    void
    testDefaults()
    {
        detail::flow_window fw;
        BEAST_EXPECT(fw.size() == default_initial_window_size);

        detail::flow_window fw2(1000);
        BEAST_EXPECT(fw2.size() == 1000);
    }

    void
    testConsume()
    {
        detail::flow_window fw(100);
        BEAST_EXPECT(fw.consume(30));
        BEAST_EXPECT(fw.size() == 70);
        BEAST_EXPECT(fw.consume(70));
        BEAST_EXPECT(fw.size() == 0);
    }

    void
    testCredit()
    {
        detail::flow_window fw(100);
        fw.consume(50);
        BEAST_EXPECT(fw.size() == 50);

        error_code ec;
        fw.credit(30, ec);
        BEAST_EXPECT(! ec);
        BEAST_EXPECT(fw.size() == 80);

        // Overflow
        detail::flow_window fw2(max_window_size);
        fw2.credit(1, ec);
        BEAST_EXPECT(ec == error::flow_control_error);
    }

    void
    testAdjust()
    {
        detail::flow_window fw(100);
        error_code ec;

        // Positive adjustment
        fw.adjust(50, ec);
        BEAST_EXPECT(! ec);
        BEAST_EXPECT(fw.size() == 150);

        // Negative adjustment
        fw.adjust(-200, ec);
        BEAST_EXPECT(! ec);
        BEAST_EXPECT(fw.size() == -50);

        // Overflow
        detail::flow_window fw2(max_window_size);
        fw2.adjust(1, ec);
        BEAST_EXPECT(ec == error::flow_control_error);
    }

    void
    testSendableBytes()
    {
        // Both windows positive
        {
            detail::flow_window conn(1000);
            detail::flow_window stream(500);
            auto n = detail::sendable_bytes(conn, stream, 16384);
            BEAST_EXPECT(n == 500);
        }

        // Max frame size limits
        {
            detail::flow_window conn(100000);
            detail::flow_window stream(100000);
            auto n = detail::sendable_bytes(conn, stream, 16384);
            BEAST_EXPECT(n == 16384);
        }

        // One window is zero
        {
            detail::flow_window conn(0);
            detail::flow_window stream(1000);
            auto n = detail::sendable_bytes(conn, stream, 16384);
            BEAST_EXPECT(n == 0);
        }

        // One window is negative
        {
            detail::flow_window conn(100);
            detail::flow_window stream(100);
            stream.consume(200); // Now -100
            auto n = detail::sendable_bytes(conn, stream, 16384);
            BEAST_EXPECT(n == 0);
        }
    }

    void
    run() override
    {
        testDefaults();
        testConsume();
        testCredit();
        testAdjust();
        testSendableBytes();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http2,flow_control);

} // http2
} // beast
} // boost
