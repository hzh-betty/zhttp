find_package(GTest REQUIRED)

# 单元测试
set(UNIT_TEST_SOURCES
    unit/http_parser_test.cc
    unit/http_request_test.cc
    unit/http_response_test.cc
    unit/router_test.cc
    unit/middleware_test.cc
)

add_executable(zhttp_unit_tests ${UNIT_TEST_SOURCES})
target_link_libraries(zhttp_unit_tests
    zhttp
    GTest::gtest
)
target_include_directories(zhttp_unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/zhttp/include
)

add_test(NAME zhttp_unit_tests COMMAND zhttp_unit_tests)

# 集成测试
set(INTEGRATION_TEST_SOURCES
    integration/http_server_test.cc
)

add_executable(zhttp_integration_tests ${INTEGRATION_TEST_SOURCES})
target_link_libraries(zhttp_integration_tests
    zhttp
    GTest::gtest
)
target_include_directories(zhttp_integration_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/zhttp/include
)

add_test(NAME zhttp_integration_tests COMMAND zhttp_integration_tests)
