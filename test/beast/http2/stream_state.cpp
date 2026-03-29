//
// Copyright (c) 2025 Salvatore Minonne
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http2/stream_state.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace http2 {

class stream_state_test : public unit_test::suite
{
public:
    void
    testStates()
    {
        // Verify all stream states are distinct
        BEAST_EXPECT(stream_state::idle != stream_state::open);
        BEAST_EXPECT(stream_state::open != stream_state::closed);
        BEAST_EXPECT(stream_state::half_closed_local != stream_state::half_closed_remote);
        BEAST_EXPECT(stream_state::reserved_local != stream_state::reserved_remote);

        // Verify initial state
        stream_state s = stream_state::idle;
        BEAST_EXPECT(s == stream_state::idle);

        // State transitions
        s = stream_state::open;
        BEAST_EXPECT(s == stream_state::open);

        s = stream_state::half_closed_local;
        BEAST_EXPECT(s == stream_state::half_closed_local);

        s = stream_state::half_closed_remote;
        BEAST_EXPECT(s == stream_state::half_closed_remote);

        s = stream_state::closed;
        BEAST_EXPECT(s == stream_state::closed);
    }

    void
    run() override
    {
        testStates();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http2,stream_state);

} // http2
} // beast
} // boost
