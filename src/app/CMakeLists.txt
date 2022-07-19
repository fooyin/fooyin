
configure_file(version.h.in version.h @ONLY)
add_library(FooyinVersion)
target_include_directories(FooyinVersion PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

set(SOURCES
    main.cpp
)

set(RESOURCES
    ${CMAKE_SOURCE_DIR}/data/icons.qrc
    ${CMAKE_SOURCE_DIR}/data/fonts.qrc
)

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
   FooyinVersion
   Qt${MAJOR_QT_VERSION}::Core
   Qt${MAJOR_QT_VERSION}::Widgets
   Qt${MAJOR_QT_VERSION}::Gui
   Qt${MAJOR_QT_VERSION}::Sql
   ${TAGLIB_LIBRARIES}
   ${ZLIB_LIBRARIES}
   ${LIBMPV_LIBRARIES}
   #${SDL2_LIBRARIES}
   #${FFMPEG_LIBRARIES}
)

qt_finalize_executable(${PROJECT_NAME})

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BIN})
