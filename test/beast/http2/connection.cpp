//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http2/connection.hpp>

#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/asio/io_context.hpp>

namespace boost {
namespace beast {
namespace http2 {

class connection_test : public unit_test::suite
{
public:
    void
    testConstruction()
    {
        net::io_context ioc;
        connection<test::stream> conn(ioc);

        BEAST_EXPECT(! conn.is_open());
        BEAST_EXPECT(conn.next_stream_id() == 1);
    }

    void
    testOptions()
    {
        net::io_context ioc;
        connection<test::stream> conn(ioc);

        // Set connection options
        connection_base::options opt;
        opt.initial_settings.max_concurrent_streams = 200;
        conn.set_option(opt);

        BEAST_EXPECT(conn.local_settings().max_concurrent_streams == 200);

        // Set timeout options
        connection_base::timeout t;
        t.keep_alive_pings = true;
        conn.set_option(t);
    }

    void
    testSettingsAccess()
    {
        net::io_context ioc;
        connection<test::stream> conn(ioc);

        // Default settings
        auto const& ls = conn.local_settings();
        BEAST_EXPECT(ls.header_table_size == 4096);
        BEAST_EXPECT(ls.enable_push == true);
        BEAST_EXPECT(ls.max_concurrent_streams == 100);
        BEAST_EXPECT(ls.initial_window_size == 65535);
        BEAST_EXPECT(ls.max_frame_size == 16384);
        BEAST_EXPECT(ls.max_header_list_size == 8192);

        auto const& rs = conn.remote_settings();
        BEAST_EXPECT(rs.header_table_size == 4096);
    }

    void
    run() override
    {
        testConstruction();
        testOptions();
        testSettingsAccess();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http2,connection);

} // http2
} // beast
} // boost
