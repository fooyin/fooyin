cmake_minimum_required(VERSION 3.16)

include(cmake/CheckBuildDir.cmake)

project(
    fooyin
    VERSION 0.1.0
    DESCRIPTION "A customisable music player"
    HOMEPAGE_URL "https://github.com/ludouzi/fooyin"
    LANGUAGES CXX
)

set(DATABASE_VERSION "0.6.3")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(cmake/CheckLevel.cmake)
include(cmake/CPM.cmake)
include(cmake/Fooyin.cmake)

include(FeatureSummary)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE
        "Release"
        CACHE STRING "" FORCE
    )
endif()

fooyin_option(FOOYIN_STATIC "Build fooyin libraries as static." OFF)
fooyin_option(BUILD_TESTS "Build fooyin tests." OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Sql Concurrent)

CPMFindPackage(
  NAME Taglib
  VERSION 1.12
  GITHUB_REPOSITORY taglib/taglib
  GIT_TAG master
  OPTIONS "CMAKE_BUILD_TYPE Release"
          "BUILD_SHARED_LIBS ON"
          "BUILD_TESTING OFF"
          "BUILD_EXAMPLES OFF"
          "BUILD_BINDINGS OFF"
          "CMAKE_POSITION_INDEPENDENT_CODE ON"
)
add_library(Taglib::Taglib ALIAS tag)

CPMFindPackage(
  NAME QCoro6
  VERSION 0.9
  GITHUB_REPOSITORY danvratil/qcoro
  GIT_TAG v0.9.0
  FIND_PACKAGE_ARGUMENTS "COMPONENTS Core"
  OPTIONS "BUILD_TESTING OFF"
          "BUILD_SHARED_LIBS ON"
          "QCORO_WITH_QTWEBSOCKETS OFF"
          "QCORO_DISABLE_DEPRECATED_TASK_H ON"
)
qcoro_enable_coroutines()

find_package(Libmpv REQUIRED)

# find_package(FFmpeg COMPONENTS AVCODEC AVFORMAT AVUTIL AVDEVICE AVFILTER
# SWSCALE SWRESAMPLE )

# ---- Fooyin library ----

configure_file(src/app/version.h.in app/version.h @ONLY)
add_library(fooyin_version OBJECT)
add_library(fooyin::version ALIAS fooyin_version)
target_include_directories(
    fooyin_version
    PUBLIC "${PROJECT_BINARY_DIR}/app"
)

add_library(fooyin_lib OBJECT src/app/application.cpp src/app/singleinstance.cpp)
add_library(fooyin::lib ALIAS fooyin_lib)

target_include_directories(
    fooyin_lib
    PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/app>"
)

add_subdirectory(src)

target_link_libraries(
    fooyin_lib
    PUBLIC fooyin::core
           fooyin::gui
           fooyin::utils
    # ${SDL2_LIBRARIES} ${FFMPEG_LIBRARIES}
)

target_compile_features(fooyin_lib PUBLIC cxx_std_20)
target_compile_options(fooyin_lib PRIVATE -Werror -Wall -Wextra -Wpedantic)

# ---- Fooyin testing ----

if(BUILD_TESTS)
    CPMFindPackage(
            NAME GTest
            VERSION 1.13.0
            GITHUB_REPOSITORY google/googletest
            GIT_TAG v1.14.0
            OPTIONS "gtest_force_shared_crt ON"
                    "BUILD_TESTING OFF"
    )
    add_subdirectory(tests)
endif()

# ---- Fooyin executable ----

set(SOURCES src/app/main.cpp)

qt_add_resources(SOURCES data/icons.qrc)

add_executable(fooyin_exe ${SOURCES})
add_executable(fooyin::exe ALIAS fooyin_exe)

set_target_properties(
    fooyin_exe
    PROPERTIES OUTPUT_NAME fooyin
               RUNTIME_OUTPUT_DIRECTORY ${FOOYIN_BINARY_OUTPUT_DIR}
               INSTALL_RPATH ${FOOYIN_LIBRARY_INSTALL_PATH}
)
target_compile_features(fooyin_exe PUBLIC cxx_std_20)
target_link_libraries(
    fooyin_exe
    PRIVATE fooyin_version
            fooyin_lib
)

# ---- Fooyin data ----

set(SHARE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX}/share)
install(FILES ${CMAKE_SOURCE_DIR}/dist/fooyin.desktop DESTINATION ${SHARE_INSTALL_PREFIX}/applications)
set(ICON_SRC_PATH ${CMAKE_SOURCE_DIR}/data/icons)
set(ICON_SIZE
    16
    22
    32
    48
    64
    128
    256
    512
)
foreach(SIZE ${ICON_SIZE})
    install(
        FILES ${ICON_SRC_PATH}/${SIZE}-fooyin.png
        DESTINATION ${SHARE_INSTALL_PREFIX}/icons/hicolor/${SIZE}x${SIZE}/apps
        RENAME fooyin.png
    )
endforeach(SIZE)
install(
    FILES ${ICON_SRC_PATH}/sc-fooyin.svg
    DESTINATION ${SHARE_INSTALL_PREFIX}/icons/hicolor/scalable/apps
    RENAME fooyin.svg
)

# ---- Fooyin install ----

install(TARGETS fooyin_exe RUNTIME COMPONENT fooyin_runtime)
