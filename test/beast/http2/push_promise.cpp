//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http2/impl/push_promise.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace http2 {

class push_promise_test : public unit_test::suite
{
public:
    void
    testPushPromiseHandler()
    {
        // Verify push_promise_handler is constructible
        push_promise_handler handler;
        BEAST_EXPECT(! handler);

        // Set a handler
        bool called = false;
        handler = [&](std::uint32_t,
            http::request<http::empty_body> const&)
        {
            called = true;
        };
        BEAST_EXPECT(static_cast<bool>(handler));
    }

    void
    run() override
    {
        testPushPromiseHandler();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http2,push_promise);

} // http2
} // beast
} // boost
