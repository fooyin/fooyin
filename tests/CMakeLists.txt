enable_testing(true)

include(GoogleTest)

#find_package(
#    Qt6
#    COMPONENTS
#    Test
#)
#set_package_properties(Qt6Test PROPERTIES TYPE OPTIONAL)

function(fooyin_add_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} GTest::gtest_main)
    gtest_discover_tests(test_scriptparser)
endfunction()

fooyin_add_test(test_scriptparser scriptparsertest.cpp)
target_link_libraries(test_scriptparser
    Qt6::Core
    fooyin_core
)

