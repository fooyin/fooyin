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
                    Fooyin::GuiPrivate
                    Fooyin::Utils
                    Qt6::Test
                    GTest::gtest_main
    )
    gtest_discover_tests(${name})
endfunction()

fooyin_add_test(test_audioconverter core/engine/audioconvertertest.cpp)
fooyin_add_test(test_audioclock core/engine/audioclocktest.cpp)
fooyin_add_test(test_audioloader core/engine/audioloadertest.cpp)
fooyin_add_test(test_audioengine core/engine/audioenginetest.cpp)
fooyin_add_test(test_visualisationbackend core/engine/visualisationbackendtest.cpp)
fooyin_add_test(test_audiomixer core/engine/audiomixertest.cpp)
if(BUILD_SENSITIVE_TESTING)
    fooyin_add_test(test_audioengine_sensitive core/engine/audioenginetest_sensitive.cpp)
    fooyin_add_test(test_audiopipeline core/engine/audiopipelinetest.cpp)
endif()
fooyin_add_test(test_timedaudiofifo core/engine/timedaudiofifotest.cpp)
fooyin_add_test(test_dspchain core/engine/dspchaintest.cpp)
fooyin_add_test(test_realfft core/engine/realffttest.cpp)
fooyin_add_test(test_enginetaskqueue core/engine/enginetaskqueuetest.cpp)
fooyin_add_test(test_fadecontroller core/engine/fadecontrollertest.cpp)
fooyin_add_test(test_outputfader core/engine/outputfadertest.cpp)
fooyin_add_test(test_lockfreeringbuffer core/engine/lockfreeringbuffertest.cpp)
fooyin_add_test(test_playbackintentreducer core/engine/playbackintentreducertest.cpp)
fooyin_add_test(test_positioncoordinator core/engine/positioncoordinatortest.cpp)
fooyin_add_test(test_playbacktransitioncoordinator core/engine/playbacktransitioncoordinatortest.cpp)
fooyin_add_test(test_replaygainprocessor core/engine/replaygainprocessortest.cpp)
fooyin_add_test(test_seekplanner core/engine/seekplannertest.cpp)
fooyin_add_test(test_trackloadplanner core/engine/trackloadplannertest.cpp)

fooyin_add_test(test_playbackcursor core/playback/playbackcursortest.cpp)
fooyin_add_test(test_playbackprogresstracker core/playback/playbackprogresstrackertest.cpp)
fooyin_add_test(test_playbacksession core/playback/playbacksessiontest.cpp)
fooyin_add_test(test_playercontroller core/playback/playercontrollertest.cpp)
fooyin_add_test(test_playlistnavigator core/playback/playlistnavigatortest.cpp)

fooyin_add_test(test_cueparser core/playlist/cueparsertest.cpp data/playlists.qrc)
fooyin_add_test(test_m3uparser core/playlist/m3uparsertest.cpp data/playlists.qrc)
fooyin_add_test(test_playlistchangeset core/playlist/playlistchangesettest.cpp)
fooyin_add_test(test_playlisthandler core/playlist/playlisthandlertest.cpp)
fooyin_add_test(test_libraryscanner core/library/libraryscannertest.cpp ${CMAKE_SOURCE_DIR}/data/data.qrc)
fooyin_add_test(test_unifiedmusiclibrary core/library/unifiedmusiclibrarytest.cpp ${CMAKE_SOURCE_DIR}/data/data.qrc)

fooyin_add_test(test_scriptparser core/scriptparsertest.cpp)
fooyin_add_test(test_stringpool core/stringpooltest.cpp)
fooyin_add_test(test_track core/tracktest.cpp)

fooyin_add_test(test_tagreader core/tagging/tagreadertest.cpp data/audio.qrc)
fooyin_add_test(test_ratingtagpolicy core/tagging/ratingtagpolicytest.cpp)
fooyin_add_test(test_tagwriter core/tagging/tagwritertest.cpp data/audio.qrc)

fooyin_add_test(test_guiutils gui/guiutilstest.cpp)
fooyin_add_test(test_scriptformatter gui/scriptformattertest.cpp)

fooyin_add_test(test_filtercontroller plugins/filters/filtercontrollertest.cpp)
target_link_libraries(test_filtercontroller PRIVATE Fooyin::FiltersInternal)
fooyin_add_test(test_filterpipeline plugins/filters/filterpipelinetest.cpp)
target_link_libraries(test_filterpipeline PRIVATE Fooyin::FiltersInternal)

fooyin_add_test(test_tagfillpattern plugins/tageditor/tagfillpatterntest.cpp ${CMAKE_SOURCE_DIR}/src/plugins/tageditor/tagfillpattern.cpp)

fooyin_add_test(test_lyricsparser plugins/lyrics/lyricsparsertest.cpp ${CMAKE_SOURCE_DIR}/src/plugins/lyrics/lyricsparser.cpp)
target_include_directories(test_lyricsparser PRIVATE ${CMAKE_SOURCE_DIR}/src/plugins/lyrics)
