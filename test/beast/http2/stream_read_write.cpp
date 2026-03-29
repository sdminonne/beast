//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header files are self-contained.
#include <boost/beast/http2/impl/stream_read.hpp>
#include <boost/beast/http2/impl/stream_write.hpp>

#include <boost/beast/http/string_body.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/asio/io_context.hpp>

namespace boost {
namespace beast {
namespace http2 {

class stream_read_write_test : public unit_test::suite
{
public:
    void
    testHeaders()
    {
        // This test verifies that stream_read.hpp and
        // stream_write.hpp compile correctly
        BEAST_EXPECT(true);
    }

    void
    run() override
    {
        testHeaders();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http2,stream_read_write);

} // http2
} // beast
} // boost
