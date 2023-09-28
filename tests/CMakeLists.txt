enable_testing(true)

include(GoogleTest)

function(fooyin_add_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE GTest::gtest_main)
    gtest_discover_tests(test_scriptparser)
endfunction()

fooyin_add_test(test_scriptparser scriptparsertest.cpp)
target_link_libraries(
    test_scriptparser
    PRIVATE Qt6::Core
            fooyin_core
)
