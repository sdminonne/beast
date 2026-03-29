//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http2/error.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <memory>

namespace boost {
namespace beast {
namespace http2 {

class error_test : public unit_test::suite
{
public:
    void
    check(char const* name, error ev)
    {
        auto const ec = make_error_code(ev);
        auto const ec_http2 = make_error_code(
            static_cast<http2::error>(0));
        auto const& cat = ec_http2.category();
        BEAST_EXPECT(std::string(ec.category().name()) == name);
        BEAST_EXPECT(! ec.message().empty());
        BEAST_EXPECT(
            std::addressof(ec.category()) == std::addressof(cat));
        BEAST_EXPECT(cat.equivalent(
            static_cast<std::underlying_type<error>::type>(ev),
                ec.category().default_error_condition(
                    static_cast<std::underlying_type<error>::type>(ev))));
        BEAST_EXPECT(cat.equivalent(ec,
            static_cast<std::underlying_type<error>::type>(ev)));

        BEAST_EXPECT(ec.message() != "");
    }

    void
    run() override
    {
        // RFC 9113 Section 7
        check("beast.http2", error::no_error);
        check("beast.http2", error::protocol_error);
        check("beast.http2", error::internal_error);
        check("beast.http2", error::flow_control_error);
        check("beast.http2", error::settings_timeout);
        check("beast.http2", error::stream_closed);
        check("beast.http2", error::frame_size_error);
        check("beast.http2", error::refused_stream);
        check("beast.http2", error::cancel);
        check("beast.http2", error::compression_error);
        check("beast.http2", error::connect_error);
        check("beast.http2", error::enhance_your_calm);
        check("beast.http2", error::inadequate_security);
        check("beast.http2", error::http_1_1_required);

        // Beast-specific
        check("beast.http2", error::bad_preface);
        check("beast.http2", error::bad_frame_header);
        check("beast.http2", error::frame_too_large);
        check("beast.http2", error::unexpected_frame);
        check("beast.http2", error::bad_header_block);
        check("beast.http2", error::hpack_integer_overflow);
        check("beast.http2", error::hpack_bad_index);
        check("beast.http2", error::hpack_table_size_exceeded);
        check("beast.http2", error::hpack_bad_huffman);
        check("beast.http2", error::bad_pseudo_header);
        check("beast.http2", error::expected_continuation);
        check("beast.http2", error::no_preface);
        check("beast.http2", error::too_many_streams);
        check("beast.http2", error::window_size_overflow);
        check("beast.http2", error::goaway_received);
        check("beast.http2", error::need_more);
        check("beast.http2", error::end_of_stream);
    }
};

BEAST_DEFINE_TESTSUITE(beast,http2,error);

} // http2
} // beast
} // boost
