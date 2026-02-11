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

fooyin_add_test(test_scriptparser scriptparsertest.cpp)
fooyin_add_test(test_scriptformatter scriptformattertest.cpp)

fooyin_add_test(test_tagreader tagreadertest.cpp data/audio.qrc)
fooyin_add_test(test_tagwriter tagwritertest.cpp data/audio.qrc)
fooyin_add_test(test_ffmpegreader ffmpegreadertest.cpp data/audio.qrc)
target_include_directories(test_ffmpegreader PRIVATE ${FFMPEG_INCLUDE_DIRS})

fooyin_add_test(test_cueparser cueparsertest.cpp data/playlists.qrc)
fooyin_add_test(test_m3uparser m3uparsertest.cpp data/playlists.qrc)
