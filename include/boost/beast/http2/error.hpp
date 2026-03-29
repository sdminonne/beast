//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_ERROR_HPP
#define BOOST_BEAST_HTTP2_ERROR_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/error.hpp>

namespace boost {
namespace beast {
namespace http2 {

/// Error codes returned from HTTP/2 algorithms and operations.
enum class error
{
    //
    // RFC 9113 Section 7 protocol error codes
    //

    /// Graceful shutdown (RFC 9113 Section 7)
    no_error = 1,

    /// Protocol error detected (RFC 9113 Section 7)
    protocol_error,

    /// Implementation fault (RFC 9113 Section 7)
    internal_error,

    /// Flow-control limits exceeded (RFC 9113 Section 7)
    flow_control_error,

    /// Settings not acknowledged in time (RFC 9113 Section 7)
    settings_timeout,

    /// Frame received for half-closed stream (RFC 9113 Section 7)
    stream_closed,

    /// Frame size is incorrect (RFC 9113 Section 7)
    frame_size_error,

    /// Stream not processed, client may retry (RFC 9113 Section 7)
    refused_stream,

    /// Stream cancelled (RFC 9113 Section 7)
    cancel,

    /// Compression state not updated (RFC 9113 Section 7)
    compression_error,

    /// TCP connection established for CONNECT failed (RFC 9113 Section 7)
    connect_error,

    /// Peer is generating excessive load (RFC 9113 Section 7)
    enhance_your_calm,

    /// Negotiated TLS parameters not acceptable (RFC 9113 Section 7)
    inadequate_security,

    /// Use HTTP/1.1 for the request (RFC 9113 Section 7)
    http_1_1_required,

    //
    // Beast-specific error codes
    //

    /// The connection preface is invalid
    bad_preface,

    /// The frame header is malformed
    bad_frame_header,

    /// The frame payload exceeds the maximum size
    frame_too_large,

    /// An unexpected frame type was received
    unexpected_frame,

    /// The header block is malformed
    bad_header_block,

    /// HPACK integer encoding overflow
    hpack_integer_overflow,

    /// HPACK invalid table index
    hpack_bad_index,

    /// HPACK dynamic table size exceeded
    hpack_table_size_exceeded,

    /// HPACK invalid Huffman encoding
    hpack_bad_huffman,

    /// Invalid pseudo-header field
    bad_pseudo_header,

    /// Expected a CONTINUATION frame
    expected_continuation,

    /// No connection preface received
    no_preface,

    /// Maximum concurrent streams exceeded
    too_many_streams,

    /// Window size overflow
    window_size_overflow,

    /// GOAWAY frame received
    goaway_received,

    /// Additional data is required
    need_more,

    /// End of stream reached
    end_of_stream
};

} // http2
} // beast
} // boost

#include <boost/beast/http2/impl/error.hpp>
#ifdef BOOST_BEAST_HEADER_ONLY
#include <boost/beast/http2/impl/error.ipp>
#endif

#endif
