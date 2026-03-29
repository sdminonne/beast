#
# Copyright (c) 2025 Salvatore Minonne
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

file(GLOB_RECURSE PFILES CONFIGURE_DEPENDS Jamfile *.cpp *.hpp)
list(FILTER PFILES EXCLUDE REGEX "h2spec_server\\.cpp$")
list(FILTER PFILES EXCLUDE REGEX "h2spec_client\\.cpp$")

add_executable(boost_beast_tests_http2 ${PFILES})

source_group("" FILES ${PFILES})

target_link_libraries(boost_beast_tests_http2
    boost_beast_lib_test
    Boost::filesystem)

set_target_properties(boost_beast_tests_http2
    PROPERTIES FOLDER "tests")

add_test(NAME boost_beast_tests_http2 COMMAND boost_beast_tests_http2)
add_dependencies(tests boost_beast_tests_http2)

# Standalone h2spec conformance test server
add_executable(h2spec_server h2spec_server.cpp)
target_link_libraries(h2spec_server
    boost_beast_lib_beast
    boost_beast_lib_asio)

# Standalone h2spec client conformance test
add_executable(h2spec_client h2spec_client.cpp)
target_link_libraries(h2spec_client
    boost_beast_lib_beast
    boost_beast_lib_asio)
