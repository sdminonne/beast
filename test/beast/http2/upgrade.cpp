//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http2/upgrade.hpp>

#include <boost/beast/http/string_body.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace http2 {

class upgrade_test : public unit_test::suite
{
public:
    void
    testIsH2cUpgrade()
    {
        // Not an upgrade request
        {
            http::request<http::string_body> req;
            req.method(http::verb::get);
            req.target("/");
            BEAST_EXPECT(! is_h2c_upgrade(req));
        }

        // Upgrade request
        {
            http::request<http::string_body> req;
            req.method(http::verb::get);
            req.target("/");
            req.set(http::field::upgrade, "h2c");
            req.set("HTTP2-Settings", "");
            req.set(http::field::connection, "Upgrade, HTTP2-Settings");
            BEAST_EXPECT(is_h2c_upgrade(req));
        }

        // Wrong upgrade protocol
        {
            http::request<http::string_body> req;
            req.method(http::verb::get);
            req.target("/");
            req.set(http::field::upgrade, "websocket");
            BEAST_EXPECT(! is_h2c_upgrade(req));
        }
    }

    void
    testPrepareUpgrade()
    {
        http::request<http::string_body> req;
        req.method(http::verb::get);
        req.target("/");
        req.set(http::field::host, "example.com");

        prepare_h2c_upgrade(req);

        BEAST_EXPECT(req[http::field::upgrade] == "h2c");
        BEAST_EXPECT(req[http::field::connection] == "Upgrade, HTTP2-Settings");
        auto it = req.find("HTTP2-Settings");
        BEAST_EXPECT(it != req.end());
    }

    void
    testPrepareUpgradeResponse()
    {
        http::response<http::string_body> res;
        prepare_h2c_upgrade_response(res);

        BEAST_EXPECT(res.result() == http::status::switching_protocols);
        BEAST_EXPECT(res[http::field::upgrade] == "h2c");
        BEAST_EXPECT(res[http::field::connection] == "Upgrade");
    }

    void
    run() override
    {
        testIsH2cUpgrade();
        testPrepareUpgrade();
        testPrepareUpgradeResponse();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http2,upgrade);

} // http2
} // beast
} // boost
