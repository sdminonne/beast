//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http2/stream.hpp>

#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/asio/io_context.hpp>

namespace boost {
namespace beast {
namespace http2 {

class stream_test : public unit_test::suite
{
public:
    void
    testStreamData()
    {
        // Test stream_data defaults
        detail::stream_data sd;
        BEAST_EXPECT(sd.id == 0);
        BEAST_EXPECT(sd.state == stream_state::idle);
        BEAST_EXPECT(sd.send_window == default_initial_window_size);
        BEAST_EXPECT(sd.recv_window == default_initial_window_size);
        BEAST_EXPECT(sd.weight == 16);
        BEAST_EXPECT(sd.dependency == 0);
        BEAST_EXPECT(sd.exclusive == false);
        BEAST_EXPECT(sd.headers_received == false);
        BEAST_EXPECT(sd.end_stream_received == false);
        BEAST_EXPECT(sd.end_stream_sent == false);
        BEAST_EXPECT(sd.rst_sent == false);
        BEAST_EXPECT(sd.rst_received == false);
    }

    void
    testStreamTable()
    {
        detail::stream_table table;

        // Create streams
        auto sd1 = table.create(1);
        BEAST_EXPECT(sd1->id == 1);
        BEAST_EXPECT(sd1->state == stream_state::idle);

        auto sd3 = table.create(3);
        BEAST_EXPECT(sd3->id == 3);

        // Find streams
        auto found = table.find(1);
        BEAST_EXPECT(found != nullptr);
        BEAST_EXPECT(found->id == 1);

        auto not_found = table.find(5);
        BEAST_EXPECT(not_found == nullptr);

        // Active count
        sd1->state = stream_state::open;
        sd3->state = stream_state::open;
        BEAST_EXPECT(table.active_count() == 2);

        // Close and cleanup
        sd1->state = stream_state::closed;
        table.cleanup();
        BEAST_EXPECT(table.find(1) == nullptr);
        BEAST_EXPECT(table.find(3) != nullptr);

        // Adjust initial window
        table.adjust_initial_window(100);
        BEAST_EXPECT(sd3->send_window == default_initial_window_size + 100);

        // Erase
        table.erase(3);
        BEAST_EXPECT(table.find(3) == nullptr);
    }

    void
    testStream()
    {
        // Create a stream handle
        net::io_context ioc;
        auto impl = boost::make_shared<
            detail::connection_impl<test::stream>>(ioc);
        auto sd = boost::make_shared<detail::stream_data>();
        sd->id = 1;
        sd->state = stream_state::open;

        stream<test::stream> s(impl, sd);

        BEAST_EXPECT(s.id() == 1);
        BEAST_EXPECT(s.state() == stream_state::open);
        BEAST_EXPECT(s.is_open());

        // Close the stream
        sd->state = stream_state::closed;
        BEAST_EXPECT(s.state() == stream_state::closed);
        BEAST_EXPECT(! s.is_open());
    }

    void
    run() override
    {
        testStreamData();
        testStreamTable();
        testStream();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http2,stream);

} // http2
} // beast
} // boost
