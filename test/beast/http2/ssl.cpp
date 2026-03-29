//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http2/ssl.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <cstring>

namespace boost {
namespace beast {
namespace http2 {

class ssl_test : public unit_test::suite
{
public:
    void
    testAlpnConstants()
    {
        BEAST_EXPECT(std::strcmp(alpn_h2, "h2") == 0);
        BEAST_EXPECT(std::strcmp(alpn_h2c, "h2c") == 0);
    }

    void
    run() override
    {
        testAlpnConstants();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http2,ssl);

} // http2
} // beast
} // boost
