//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_DETAIL_CONNECTION_IMPL_HPP
#define BOOST_BEAST_HTTP2_DETAIL_CONNECTION_IMPL_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http2/error.hpp>
#include <boost/beast/http2/frame.hpp>
#include <boost/beast/http2/settings.hpp>
#include <boost/beast/http2/connection_base.hpp>
#include <boost/beast/http2/hpack/decoder.hpp>
#include <boost/beast/http2/hpack/encoder.hpp>
#include <boost/shared_ptr.hpp>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace boost {
namespace beast {
namespace http2 {
namespace detail {

// Forward declaration
struct stream_data;

/** Shared connection implementation state.

    Owned via shared_ptr by the connection and all its streams.
    Pattern follows websocket::stream::impl_type.
*/
template<class NextLayer>
struct connection_impl
{
    // The underlying stream
    NextLayer stream;

    // HPACK codec (per-connection)
    hpack::encoder hpack_encoder;
    hpack::decoder hpack_decoder;

    // Local and remote settings
    settings local_settings;
    settings remote_settings;

    // Connection state
    bool preface_sent = false;
    bool preface_received = false;
    bool goaway_sent = false;
    bool goaway_received = false;
    bool is_server = false;

    // Stream management
    std::uint32_t next_stream_id = 1; // Client starts with odd IDs
    std::uint32_t last_peer_stream_id = 0;
    std::uint32_t goaway_last_stream_id = 0;
    std::uint32_t goaway_error_code = 0;

    // Stream table
    std::unordered_map<std::uint32_t, boost::shared_ptr<stream_data>> streams;

    // Flow control - connection-level windows
    std::int32_t send_window = default_initial_window_size;
    std::int32_t recv_window = default_initial_window_size;

    // Read buffer
    flat_buffer rd_buf;

    // Header block reassembly buffer (for CONTINUATION frames)
    std::vector<std::uint8_t> header_block_buf;
    std::uint32_t header_block_stream_id = 0;
    bool expecting_continuation = false;

    // Timeout & options
    connection_base::timeout timeout_opt;
    connection_base::options conn_opt;

    // Constructor
    template<class... Args>
    explicit
    connection_impl(Args&&... args)
        : stream(std::forward<Args>(args)...)
    {
    }
};

} // detail
} // http2
} // beast
} // boost

#endif
