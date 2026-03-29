//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_IMPL_ERROR_HPP
#define BOOST_BEAST_HTTP2_IMPL_ERROR_HPP

#include <type_traits>

namespace boost {
namespace system {
template<>
struct is_error_code_enum<::boost::beast::http2::error>
{
    static bool const value = true;
};
} // system
} // boost

namespace boost {
namespace beast {
namespace http2 {

BOOST_BEAST_DECL
error_code
make_error_code(error ev);

} // http2
} // beast
} // boost

#endif
