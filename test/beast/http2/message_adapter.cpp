//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http2/message_adapter.hpp>

#include <boost/beast/http/string_body.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace http2 {

class message_adapter_test : public unit_test::suite
{
public:
    void
    testRequestToHeaderList()
    {
        // GET request
        {
            http::request<http::string_body> req;
            req.method(http::verb::get);
            req.target("/index.html");
            req.set(http::field::host, "www.example.com");
            req.set(http::field::user_agent, "beast/http2");

            header_list headers;
            request_to_header_list(req, headers);

            // Check pseudo-headers come first
            BEAST_EXPECT(headers.size() >= 3);
            BEAST_EXPECT(headers[0].first == ":method");
            BEAST_EXPECT(headers[0].second == "GET");
            BEAST_EXPECT(headers[1].first == ":scheme");
            BEAST_EXPECT(headers[1].second == "https");
            BEAST_EXPECT(headers[2].first == ":path");
            BEAST_EXPECT(headers[2].second == "/index.html");

            // Find :authority
            bool found_authority = false;
            for(auto const& h : headers)
            {
                if(h.first == ":authority")
                {
                    BEAST_EXPECT(h.second == "www.example.com");
                    found_authority = true;
                }
            }
            BEAST_EXPECT(found_authority);
        }

        // POST request
        {
            http::request<http::string_body> req;
            req.method(http::verb::post);
            req.target("/submit");
            req.set(http::field::host, "example.com");
            req.set(http::field::content_type, "application/json");

            header_list headers;
            request_to_header_list(req, headers);

            BEAST_EXPECT(headers[0].first == ":method");
            BEAST_EXPECT(headers[0].second == "POST");
        }

        // CONNECT method (no :scheme or :path)
        {
            http::request<http::string_body> req;
            req.method(http::verb::connect);
            req.target("server.example.com:443");

            header_list headers;
            request_to_header_list(req, headers);

            bool found_scheme = false;
            bool found_path = false;
            for(auto const& h : headers)
            {
                if(h.first == ":scheme") found_scheme = true;
                if(h.first == ":path") found_path = true;
            }
            BEAST_EXPECT(!found_scheme);
            BEAST_EXPECT(!found_path);
        }
    }

    void
    testResponseToHeaderList()
    {
        http::response<http::string_body> res;
        res.result(http::status::ok);
        res.set(http::field::content_type, "text/html");
        res.set(http::field::server, "beast/http2");

        header_list headers;
        response_to_header_list(res, headers);

        BEAST_EXPECT(headers.size() >= 1);
        BEAST_EXPECT(headers[0].first == ":status");
        BEAST_EXPECT(headers[0].second == "200");
    }

    void
    testHeaderListToRequest()
    {
        header_list headers;
        headers.emplace_back(":method", "GET");
        headers.emplace_back(":scheme", "https");
        headers.emplace_back(":path", "/index.html");
        headers.emplace_back(":authority", "www.example.com");
        headers.emplace_back("user-agent", "beast/http2");

        http::request<http::string_body> req;
        error_code ec;
        header_list_to_request(headers, req, ec);

        BEAST_EXPECT(! ec);
        BEAST_EXPECT(req.method() == http::verb::get);
        BEAST_EXPECT(req.target() == "/index.html");
        BEAST_EXPECT(req[http::field::host] == "www.example.com");
        BEAST_EXPECT(req["user-agent"] == "beast/http2");
        BEAST_EXPECT(req.version() == 20);
    }

    void
    testHeaderListToResponse()
    {
        header_list headers;
        headers.emplace_back(":status", "200");
        headers.emplace_back("content-type", "text/html");

        http::response<http::string_body> res;
        error_code ec;
        header_list_to_response(headers, res, ec);

        BEAST_EXPECT(! ec);
        BEAST_EXPECT(res.result() == http::status::ok);
        BEAST_EXPECT(res["content-type"] == "text/html");
        BEAST_EXPECT(res.version() == 20);
    }

    void
    testErrors()
    {
        // Missing :method
        {
            header_list headers;
            headers.emplace_back(":path", "/");

            http::request<http::string_body> req;
            error_code ec;
            header_list_to_request(headers, req, ec);
            BEAST_EXPECT(ec == error::bad_pseudo_header);
        }

        // Missing :status
        {
            header_list headers;
            headers.emplace_back("content-type", "text/html");

            http::response<http::string_body> res;
            error_code ec;
            header_list_to_response(headers, res, ec);
            BEAST_EXPECT(ec == error::bad_pseudo_header);
        }

        // Unknown pseudo-header in request
        {
            header_list headers;
            headers.emplace_back(":method", "GET");
            headers.emplace_back(":path", "/");
            headers.emplace_back(":unknown", "value");

            http::request<http::string_body> req;
            error_code ec;
            header_list_to_request(headers, req, ec);
            BEAST_EXPECT(ec == error::bad_pseudo_header);
        }
    }

    void
    testRoundTrip()
    {
        // Request round-trip
        {
            http::request<http::string_body> req;
            req.method(http::verb::get);
            req.target("/index.html");
            req.set(http::field::host, "www.example.com");
            req.set(http::field::accept, "text/html");

            header_list headers;
            request_to_header_list(req, headers);

            http::request<http::string_body> req2;
            error_code ec;
            header_list_to_request(headers, req2, ec);

            BEAST_EXPECT(! ec);
            BEAST_EXPECT(req2.method() == http::verb::get);
            BEAST_EXPECT(req2.target() == "/index.html");
            BEAST_EXPECT(req2[http::field::host] == "www.example.com");
        }

        // Response round-trip
        {
            http::response<http::string_body> res;
            res.result(http::status::not_found);
            res.set(http::field::content_type, "text/html");

            header_list headers;
            response_to_header_list(res, headers);

            http::response<http::string_body> res2;
            error_code ec;
            header_list_to_response(headers, res2, ec);

            BEAST_EXPECT(! ec);
            BEAST_EXPECT(res2.result() == http::status::not_found);
        }
    }

    void
    run() override
    {
        testRequestToHeaderList();
        testResponseToHeaderList();
        testHeaderListToRequest();
        testHeaderListToResponse();
        testErrors();
        testRoundTrip();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http2,message_adapter);

} // http2
} // beast
} // boost
