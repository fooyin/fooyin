enable_testing(true)

add_executable(test_scriptparser
  scriptparsertest.cpp
)

target_link_libraries(
  test_scriptparser
  GTest::gtest_main
  Qt6::Core
  fooyin_core
)

include(GoogleTest)

gtest_discover_tests(test_scriptparser)
