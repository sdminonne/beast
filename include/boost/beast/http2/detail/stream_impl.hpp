//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_DETAIL_STREAM_IMPL_HPP
#define BOOST_BEAST_HTTP2_DETAIL_STREAM_IMPL_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http2/frame.hpp>
#include <boost/beast/http2/stream_state.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <cstdint>
#include <unordered_map>

namespace boost {
namespace beast {
namespace http2 {
namespace detail {

/** Per-stream state data.

    Each HTTP/2 stream has its own state, flow control windows,
    inbound data buffer, and flags tracking which protocol
    events have occurred.
*/
struct stream_data
{
    /// Stream identifier
    std::uint32_t id = 0;

    /// Current stream state
    stream_state state = stream_state::idle;

    /// Send flow-control window
    std::int32_t send_window = default_initial_window_size;

    /// Receive flow-control window
    std::int32_t recv_window = default_initial_window_size;

    /// Priority weight (1-256, default 16)
    std::uint8_t weight = 16;

    /// Stream dependency
    std::uint32_t dependency = 0;

    /// Whether the dependency is exclusive
    bool exclusive = false;

    /// Whether headers have been received
    bool headers_received = false;

    /// Whether trailers have been received
    bool trailers_received = false;

    /// Whether END_STREAM has been received
    bool end_stream_received = false;

    /// Whether END_STREAM has been sent
    bool end_stream_sent = false;

    /// Whether RST_STREAM has been sent
    bool rst_sent = false;

    /// Whether RST_STREAM has been received
    bool rst_received = false;

    /// RST_STREAM error code (if received)
    std::uint32_t rst_error_code = 0;

    /// Inbound data buffer (populated by the read pump)
    flat_buffer rd_buf;
};

/** Stream table.

    Manages the collection of active streams within a connection.
*/
class stream_table
{
    std::unordered_map<std::uint32_t,
        boost::shared_ptr<stream_data>> streams_;
    std::size_t active_count_ = 0;

public:
    /// Create a new stream with the given ID
    boost::shared_ptr<stream_data>
    create(std::uint32_t id)
    {
        auto sd = boost::make_shared<stream_data>();
        sd->id = id;
        sd->state = stream_state::idle;
        streams_[id] = sd;
        return sd;
    }

    /// Find a stream by ID
    boost::shared_ptr<stream_data>
    find(std::uint32_t id) const
    {
        auto it = streams_.find(id);
        if(it == streams_.end())
            return nullptr;
        return it->second;
    }

    /// Remove a stream
    void
    erase(std::uint32_t id)
    {
        streams_.erase(id);
    }

    /// Remove closed streams
    void
    cleanup()
    {
        for(auto it = streams_.begin(); it != streams_.end(); )
        {
            if(it->second->state == stream_state::closed)
                it = streams_.erase(it);
            else
                ++it;
        }
    }

    /// Adjust initial window size delta for all streams
    void
    adjust_initial_window(std::int32_t delta)
    {
        for(auto& pair : streams_)
        {
            auto& sd = pair.second;
            sd->send_window += delta;
        }
    }

    /// Get the number of active (non-closed) streams
    std::size_t
    active_count() const
    {
        std::size_t count = 0;
        for(auto const& pair : streams_)
        {
            if(pair.second->state != stream_state::closed)
                ++count;
        }
        return count;
    }

    /// Access the underlying map (for iteration)
    auto const&
    map() const noexcept
    {
        return streams_;
    }
};

} // detail
} // http2
} // beast
} // boost

#endif
