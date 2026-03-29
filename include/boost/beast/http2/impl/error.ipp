//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_IMPL_ERROR_IPP
#define BOOST_BEAST_HTTP2_IMPL_ERROR_IPP

#include <boost/beast/http2/error.hpp>
#include <type_traits>

namespace boost {
namespace beast {
namespace http2 {
namespace detail {

class http2_error_category : public error_category
{
public:
    const char*
    name() const noexcept override
    {
        return "beast.http2";
    }

    http2_error_category() : error_category(0x7a3f8e2b1d5c9a04u) {}

    BOOST_BEAST_DECL
    char const*
    message(int ev, char*, std::size_t) const noexcept override
    {
        switch(static_cast<error>(ev))
        {
        // RFC 9113 Section 7
        case error::no_error: return "no error";
        case error::protocol_error: return "protocol error";
        case error::internal_error: return "internal error";
        case error::flow_control_error: return "flow control error";
        case error::settings_timeout: return "settings timeout";
        case error::stream_closed: return "stream closed";
        case error::frame_size_error: return "frame size error";
        case error::refused_stream: return "refused stream";
        case error::cancel: return "cancel";
        case error::compression_error: return "compression error";
        case error::connect_error: return "connect error";
        case error::enhance_your_calm: return "enhance your calm";
        case error::inadequate_security: return "inadequate security";
        case error::http_1_1_required: return "HTTP/1.1 required";

        // Beast-specific
        case error::bad_preface: return "bad connection preface";
        case error::bad_frame_header: return "bad frame header";
        case error::frame_too_large: return "frame too large";
        case error::unexpected_frame: return "unexpected frame";
        case error::bad_header_block: return "bad header block";
        case error::hpack_integer_overflow: return "HPACK integer overflow";
        case error::hpack_bad_index: return "HPACK bad index";
        case error::hpack_table_size_exceeded: return "HPACK table size exceeded";
        case error::hpack_bad_huffman: return "HPACK bad Huffman encoding";
        case error::bad_pseudo_header: return "bad pseudo-header";
        case error::expected_continuation: return "expected CONTINUATION frame";
        case error::no_preface: return "no connection preface";
        case error::too_many_streams: return "too many concurrent streams";
        case error::window_size_overflow: return "window size overflow";
        case error::goaway_received: return "GOAWAY received";
        case error::need_more: return "need more data";
        case error::end_of_stream: return "end of stream";

        default:
            return "beast.http2 error";
        }
    }

    std::string
    message(int ev) const override
    {
        return message(ev, nullptr, 0);
    }

    error_condition
    default_error_condition(
        int ev) const noexcept override
    {
        return error_condition{ev, *this};
    }

    bool
    equivalent(int ev,
        error_condition const& condition
            ) const noexcept override
    {
        return condition.value() == ev &&
            &condition.category() == this;
    }

    bool
    equivalent(error_code const& error,
        int ev) const noexcept override
    {
        return error.value() == ev &&
            &error.category() == this;
    }
};

} // detail

error_code
make_error_code(error ev)
{
    static detail::http2_error_category const cat{};
    return error_code{static_cast<
        std::underlying_type<error>::type>(ev), cat};
}

} // http2
} // beast
} // boost

#endif
