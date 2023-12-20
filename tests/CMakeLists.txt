enable_testing(true)

include(GoogleTest)

function(fooyin_add_test name)
add_executable(${name} ${ARGN} testutils.cpp)
    target_link_libraries(
            ${name}
            PRIVATE Fooyin::Core
                    Fooyin::CorePrivate
                    GTest::gtest_main
    )
    gtest_discover_tests(${name})
endfunction()

fooyin_add_test(test_scriptparser scriptparsertest.cpp)

qt_add_resources(TEST_SOURCES data/audio.qrc)
add_library(fooyin_test_data ${TEST_SOURCES})

fooyin_add_test(test_tagreader tagreadertest.cpp)
target_link_libraries(
    test_tagreader
    PRIVATE fooyin_test_data
)

fooyin_add_test(test_tagwriter tagwritertest.cpp)
target_link_libraries(
    test_tagwriter
    PRIVATE fooyin_test_data
)
