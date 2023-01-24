
configure_file(version.h.in version.h @ONLY)
add_library(Fooyin_Version)
target_include_directories(Fooyin_Version PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

set(SOURCES
    main.cpp
    singleinstance.h
    singleinstance.cpp
    application.h
    application.cpp
)

set(RESOURCES
    ${CMAKE_SOURCE_DIR}/data/icons.qrc
)

string(TOLOWER ${PROJECT_NAME} output_name)

qt6_add_resources(SOURCES ${RESOURCES})

qt_add_executable(${PROJECT_NAME}
   MANUAL_FINALIZATION
   ${HEADERS}
   ${SOURCES}
   ${QM_FILES}
)

target_compile_options(${PROJECT_NAME} PRIVATE -Werror -Wall -Wextra)

target_link_libraries(${PROJECT_NAME}
   PRIVATE
   Qt${MAJOR_QT_VERSION}::Core
   Qt${MAJOR_QT_VERSION}::Widgets
   Qt${MAJOR_QT_VERSION}::Gui
   Qt${MAJOR_QT_VERSION}::Sql
   ${TAGLIB_LIBRARIES}
   ${ZLIB_LIBRARIES}
   ${LIBMPV_LIBRARIES}
   Fooyin_Core
   Fooyin_Gui
   #${SDL2_LIBRARIES}
   #${FFMPEG_LIBRARIES}
)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    OUTPUT_NAME ${output_name}
    RUNTIME_OUTPUT_DIRECTORY ${FOOYIN_BINARY_OUTPUT_DIR}
    INSTALL_RPATH ${FOOYIN_LIBRARY_INSTALL_PATH}
)

qt_finalize_executable(${PROJECT_NAME})

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${FOOYIN_BINARY_DIR})
