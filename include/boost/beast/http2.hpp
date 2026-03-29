//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_HPP
#define BOOST_BEAST_HTTP2_HPP

#include <boost/beast/core/detail/config.hpp>

#include <boost/beast/http2/error.hpp>
#include <boost/beast/http2/frame.hpp>
#include <boost/beast/http2/settings.hpp>
#include <boost/beast/http2/stream_state.hpp>
#include <boost/beast/http2/connection_base.hpp>
#include <boost/beast/http2/connection_fwd.hpp>
#include <boost/beast/http2/connection.hpp>
#include <boost/beast/http2/stream.hpp>
#include <boost/beast/http2/message_adapter.hpp>
#include <boost/beast/http2/upgrade.hpp>
#include <boost/beast/http2/ssl.hpp>

#include <boost/beast/http2/detail/flow_control.hpp>

#include <boost/beast/http2/hpack/integer.hpp>
#include <boost/beast/http2/hpack/huffman.hpp>
#include <boost/beast/http2/hpack/static_table.hpp>
#include <boost/beast/http2/hpack/dynamic_table.hpp>
#include <boost/beast/http2/hpack/decoder.hpp>
#include <boost/beast/http2/hpack/encoder.hpp>

#include <boost/beast/http2/impl/stream_read.hpp>
#include <boost/beast/http2/impl/stream_write.hpp>
#include <boost/beast/http2/impl/push_promise.hpp>

#endif
