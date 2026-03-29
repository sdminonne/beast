//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP2_SSL_HPP
#define BOOST_BEAST_HTTP2_SSL_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/string_type.hpp>

namespace boost {
namespace beast {
namespace http2 {

/// ALPN protocol identifier for HTTP/2 over TLS
constexpr char const* alpn_h2 = "h2";

/// ALPN protocol identifier for HTTP/2 cleartext
constexpr char const* alpn_h2c = "h2c";

#ifdef BOOST_BEAST_USE_OPENSSL

/** Set ALPN to advertise HTTP/2 on an SSL context.

    Configures the SSL context to advertise the "h2" protocol
    during TLS negotiation via the Application-Layer Protocol
    Negotiation (ALPN) extension.

    @param ctx The SSL context to configure
*/
inline
void
set_alpn_h2(boost::asio::ssl::context& ctx)
{
    // ALPN wire format: length-prefixed protocol names
    // "h2" = 0x02 'h' '2'
    static unsigned char const alpn[] = {
        2, 'h', '2'
    };
    SSL_CTX_set_alpn_protos(ctx.native_handle(), alpn, sizeof(alpn));
}

/** Check if the negotiated ALPN protocol is HTTP/2.

    @param ssl The SSL stream to check
    @return true if ALPN negotiated "h2"
*/
template<class Stream>
bool
is_alpn_h2(Stream const& ssl)
{
    unsigned char const* data = nullptr;
    unsigned int len = 0;
    SSL_get0_alpn_selected(
        ssl.native_handle(), &data, &len);
    return len == 2 && data[0] == 'h' && data[1] == '2';
}

#endif // BOOST_BEAST_USE_OPENSSL

} // http2
} // beast
} // boost

#endif
