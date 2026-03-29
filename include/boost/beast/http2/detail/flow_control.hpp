//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_DETAIL_FLOW_CONTROL_HPP
#define BOOST_BEAST_HTTP2_DETAIL_FLOW_CONTROL_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/http2/error.hpp>
#include <boost/beast/http2/frame.hpp>
#include <algorithm>
#include <cstdint>

namespace boost {
namespace beast {
namespace http2 {
namespace detail {

/** HTTP/2 flow-control window.

    Manages a single flow-control window for either a connection
    or a stream. The window tracks how many bytes can be sent
    (send window) or how many bytes the peer is allowed to send
    (receive window).

    The window value can be negative (after SETTINGS change) but
    sending is only allowed when the value is positive.
*/
class flow_window
{
    std::int32_t window_;

public:
    /** Constructor.

        @param initial Initial window size (default: 65535)
    */
    explicit
    flow_window(std::int32_t initial = default_initial_window_size)
        : window_(initial)
    {
    }

    /// Returns the current window size
    std::int32_t
    size() const noexcept
    {
        return window_;
    }

    /** Consume bytes from the window (after sending DATA).

        @param n Number of bytes consumed
        @return true if the window had enough bytes
    */
    bool
    consume(std::int32_t n) noexcept
    {
        window_ -= n;
        return true; // Window can go negative after SETTINGS change
    }

    /** Credit bytes to the window (from WINDOW_UPDATE).

        @param n Number of bytes to credit
        @param ec Set to flow_control_error if overflow
    */
    void
    credit(std::int32_t n, error_code& ec)
    {
        ec = {};
        std::int64_t new_window =
            static_cast<std::int64_t>(window_) + n;
        if(new_window > max_window_size)
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::flow_control_error);
            return;
        }
        window_ = static_cast<std::int32_t>(new_window);
    }

    /** Adjust the window size (from SETTINGS initial_window_size change).

        @param delta Change in initial window size
        @param ec Set to flow_control_error if overflow
    */
    void
    adjust(std::int32_t delta, error_code& ec)
    {
        ec = {};
        std::int64_t new_window =
            static_cast<std::int64_t>(window_) + delta;
        if(new_window > max_window_size)
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::flow_control_error);
            return;
        }
        window_ = static_cast<std::int32_t>(new_window);
    }
};

/** Calculate the maximum number of bytes that can be sent.

    Takes into account both the connection-level and stream-level
    flow control windows, as well as the maximum frame size.

    @param conn Connection-level flow window
    @param stream Stream-level flow window
    @param max_frame_size Maximum frame payload size
    @return Number of bytes that can be sent in a single frame
*/
inline
std::int32_t
sendable_bytes(
    flow_window const& conn,
    flow_window const& stream,
    std::size_t max_frame_size) noexcept
{
    std::int32_t available = std::min(conn.size(), stream.size());
    if(available <= 0)
        return 0;
    return std::min(available,
        static_cast<std::int32_t>(max_frame_size));
}

} // detail
} // http2
} // beast
} // boost

#endif
