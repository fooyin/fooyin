find_package(Ebur128 QUIET)

if(NOT Ebur128_FOUND)
    message(STATUS "Ebur128 not found; ffmpeg scanner will be used instead.")
else()
    set(HAVE_EBUR128 TRUE)
endif()

create_fooyin_plugin_internal(
    rgscanner
    DEPENDS Fooyin::CorePrivate
            Fooyin::Gui
    SOURCES rgscanner.cpp
            rgscanner.h
            rgscanresults.cpp
            rgscanresults.h
            rgscanresultsmodel.cpp
            rgscanresultsmodel.h
            rgscannerplugin.cpp
            rgscannerplugin.h
)

if(HAVE_EBUR128)
    target_link_libraries(rgscanner PRIVATE Ebur128::Ebur128)
    target_sources(rgscanner PRIVATE ebur128scanner.cpp ebur128scanner.h)
    target_compile_definitions(rgscanner PRIVATE HAVE_EBUR128)
else()
    target_link_libraries(rgscanner PRIVATE ${FFMPEG_LIBRARIES})
    target_sources(rgscanner PRIVATE ffmpegscanner.cpp ffmpegscanner.h)
    target_include_directories(rgscanner PRIVATE ${FFMPEG_INCLUDE_DIRS})
endif()

