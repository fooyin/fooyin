enable_testing(true)

include(GoogleTest)

function(fooyin_add_test name)
    add_executable(${name} ${ARGN} testutils.cpp)
    fooyin_set_rpath(${name} ${LIB_INSTALL_DIR})
    target_link_libraries(
            ${name}
            PRIVATE Fooyin::Core
                    Fooyin::CorePrivate
                    Fooyin::Gui
                    GTest::gtest_main
    )
    gtest_discover_tests(${name})
endfunction()

qt_add_resources(TEST_SOURCES data/audio.qrc)
qt_add_resources(TEST_SOURCES data/cue.qrc)
add_library(fooyin_test_data ${TEST_SOURCES})

fooyin_add_test(test_scriptparser scriptparsertest.cpp)
fooyin_add_test(test_scriptformatter scriptformattertest.cpp)

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

fooyin_add_test(test_cueparser cueparsertest.cpp)
target_link_libraries(
    test_cueparser
    PRIVATE fooyin_test_data
)
