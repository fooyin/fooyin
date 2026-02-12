enable_testing(true)

include(GoogleTest)

function(fooyin_add_test name)
    add_executable(${name} ${ARGN} testutils.cpp)
    fooyin_set_rpath(${name} ${LIB_INSTALL_DIR})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(
            ${name}
            PRIVATE Fooyin::Core
                    Fooyin::CorePrivate
                    Fooyin::Gui
                    GTest::gtest_main
    )
    gtest_discover_tests(${name})
endfunction()

fooyin_add_test(test_scriptparser core/scripting/scriptparsertest.cpp)
fooyin_add_test(test_scriptformatter core/scripting/scriptformattertest.cpp)

fooyin_add_test(test_tagreader core/tagging/tagreadertest.cpp data/audio.qrc)
fooyin_add_test(test_tagwriter core/tagging/tagwritertest.cpp data/audio.qrc)

fooyin_add_test(test_cueparser core/playlist/cueparsertest.cpp data/playlists.qrc)
fooyin_add_test(test_m3uparser core/playlist/m3uparsertest.cpp data/playlists.qrc)
