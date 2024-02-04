cmake_minimum_required(VERSION 3.16)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

include(CheckBuildDir)
include(CheckLevel)

project(
    fooyin
    VERSION 0.3.6
    DESCRIPTION "A customisable music player"
    HOMEPAGE_URL "https://github.com/ludouzi/fooyin"
    LANGUAGES CXX
)

set(FOOYIN_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(FOOYIN_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(FOOYIN_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(FOOYIN_VERSION ${PROJECT_VERSION})
set(FOOYIN_SOVERSION "0.0.0")
set(FOOYIN_PLUGIN_VERSION "0.0")

include(GenerateExportHeader)
include(FeatureSummary)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

set(FOOYIN_REQUIRED_CXX_FEATURES cxx_std_20)

include(FooyinMacrosInternal)

fooyin_option(BUILD_SHARED_LIBS "Build fooyin libraries as shared" ON)
fooyin_option(BUILD_PLUGINS "Build plugins included with fooyin" ON)
fooyin_option(BUILD_TESTING "Build fooyin tests" OFF)
fooyin_option(BUILD_CCACHE "Build using CCache if found" ON)
fooyin_option(BUILD_PCH "Build with precompiled header support" OFF)
fooyin_option(BUILD_WERROR "Build with -Werror" OFF)

list(
    APPEND
    FOOYIN_COMPILE_OPTIONS
    -Wall
    -Wextra
    -Wpedantic
    -Wunused
    -Wshadow
    -Wundef
    -Wuninitialized
    -Wredundant-decls
    -Wcast-align
    -Winit-self
    -Wmissing-include-dirs
    -Wstrict-overflow=2
    -Wunused-parameter
    -Wdisabled-optimization
    -Woverloaded-virtual
    -Wold-style-cast
)
if(BUILD_WERROR)
    list(APPEND FOOYIN_COMPILE_OPTIONS -Werror)
endif()

# ---- Dependencies ----

find_package(
    Qt6 REQUIRED
    COMPONENTS Core
               Widgets
               Gui
               Sql
               Concurrent
               Network
)
set_package_properties(Qt6Core PROPERTIES TYPE REQUIRED)
set_package_properties(Qt6Widgets PROPERTIES TYPE REQUIRED)
set_package_properties(Qt6Gui PROPERTIES TYPE REQUIRED)
set_package_properties(Qt6Sql PROPERTIES TYPE REQUIRED)
set_package_properties(Qt6Concurrent PROPERTIES TYPE REQUIRED)
set_package_properties(Qt6Network PROPERTIES TYPE REQUIRED)

find_package(QCoro6 REQUIRED COMPONENTS Core)
qcoro_enable_coroutines()

find_package(Taglib REQUIRED taglib>=1.12)
find_package(ALSA REQUIRED)

find_package(
    FFmpeg REQUIRED
    COMPONENTS AVCODEC
               AVFORMAT
               AVUTIL
               AVFILTER
               SWRESAMPLE
)

find_package(KDSingleApplication-qt6 QUIET)
if(NOT TARGET KDAB::kdsingleapplication)
    include(FetchContent)
    set(KDSingleApplication_QT6 ON CACHE BOOL INTERNAL)
    set(KDSingleApplication_EXAMPLES OFF CACHE BOOL INTERNAL)
    set(KDSingleApplication_STATIC ON CACHE BOOL INTERNAL)
    FetchContent_Declare(
        kdsingleapplication
        GIT_REPOSITORY https://github.com/KDAB/KDSingleApplication.git
        GIT_TAG v1.1.0
    )
    FetchContent_MakeAvailable(kdsingleapplication)
endif()

if(BUILD_CCACHE)
    find_program(CCACHE_EXECUTABLE ccache)
    if(CCACHE_EXECUTABLE)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_EXECUTABLE})
        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE_EXECUTABLE})
    endif()
endif()

# ---- Fooyin paths ----

include(GNUInstallDirs)

set(FOOYIN_BIN_DIR ${CMAKE_INSTALL_BINDIR})
set(FOOYIN_LIBRARY_DIR "${CMAKE_INSTALL_LIBDIR}/fooyin")
set(FOOYIN_PLUGIN_DIR "${CMAKE_INSTALL_LIBDIR}/fooyin/plugins")
set(FOOYIN_INCLUDE_DIR "${CMAKE_INSTALL_INCLUDEDIR}/fooyin")

set(FOOYIN_BIN_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/${FOOYIN_BIN_DIR})
set(FOOYIN_LIBRARY_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/${FOOYIN_LIBRARY_DIR})
set(FOOYIN_PLUGIN_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/${FOOYIN_PLUGIN_DIR})
set(FOOYIN_INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/${FOOYIN_INCLUDE_DIR})

set(FOOYIN_BINARY_OUTPUT_DIR ${PROJECT_BINARY_DIR}/${FOOYIN_BIN_DIR})
set(FOOYIN_LIBRARY_OUTPUT_DIR ${PROJECT_BINARY_DIR}/${FOOYIN_LIBRARY_DIR})
set(FOOYIN_PLUGIN_OUTPUT_DIR ${PROJECT_BINARY_DIR}/${FOOYIN_PLUGIN_DIR})
set(FOOYIN_INCLUDE_OUTPUT_DIR ${PROJECT_BINARY_DIR}/${FOOYIN_INCLUDE_DIR})

file(RELATIVE_PATH FOOYIN_RELATIVE_LIB_PATH "/${FOOYIN_BIN_DIR}" "/${FOOYIN_LIBRARY_DIR}")
file(RELATIVE_PATH FOOYIN_RELATIVE_PLUGIN_PATH "/${FOOYIN_BIN_DIR}" "/${FOOYIN_PLUGIN_DIR}")
file(RELATIVE_PATH FOOYIN_PLUGIN_TO_LIB "/${FOOYIN_PLUGIN_DIR}" "/${FOOYIN_LIBRARY_DIR}")

set(FOOYIN_RPATH "\$ORIGIN")
set(FOOYIN_LIBRARY_RPATH "\$ORIGIN")
set(FOOYIN_PLUGIN_RPATH "\$ORIGIN;\$ORIGIN/${FOOYIN_PLUGIN_TO_LIB}")

# ---- Fooyin library ----

if(BUILD_SHARED_LIBS)
    set(FOOYIN_LIBRARY_TYPE SHARED)
else()
    set(FOOYIN_LIBRARY_TYPE STATIC)
endif()

if(BUILD_PCH)
    file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/empty_pch.cpp
         CONTENT "/*dummy pch file*/"
    )
    add_library(fooyin_pch STATIC ${CMAKE_CURRENT_BINARY_DIR}/empty_pch.cpp)
    target_precompile_headers(
        fooyin_pch PRIVATE ${CMAKE_SOURCE_DIR}/src/app/pch.h
    )
    target_compile_definitions(fooyin_pch PRIVATE QT_USE_QSTRINGBUILDER)
    set_target_properties(
        fooyin_pch
        PROPERTIES POSITION_INDEPENDENT_CODE ON
                   CXX_VISIBILITY_PRESET hidden
                   VISIBILITY_INLINES_HIDDEN YES
    )
    target_link_libraries(fooyin_pch PRIVATE Qt::Core Qt::Gui Qt::Widgets)
    target_compile_features(fooyin_pch PRIVATE ${FOOYIN_REQUIRED_CXX_FEATURES})
    target_compile_options(fooyin_pch PRIVATE ${FOOYIN_COMPILE_OPTIONS})
    if(CMAKE_VERSION GREATER_EQUAL 3.19)
        set(CMAKE_PCH_INSTANTIATE_TEMPLATES ON)
    endif()
endif()

configure_file(src/app/version.h.in app/version.h @ONLY)
add_library(fooyin_version OBJECT)
target_include_directories(
    fooyin_version PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/app>
)
configure_file(src/app/config.h.in app/config.h @ONLY)
add_library(fooyin_config OBJECT)
target_include_directories(
    fooyin_config PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/app>
)

add_library(fooyin_lib OBJECT)

target_include_directories(
    fooyin_lib PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>"
                      "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/app>"
)

set_target_properties(
    fooyin_lib PROPERTIES VERSION "${FOOYIN_VERSION}" EXPORT_NAME Fooyin
)

add_subdirectory(src)

target_link_libraries(fooyin_lib PUBLIC Fooyin::Core Fooyin::Gui Fooyin::Utils)

target_compile_features(fooyin_lib PUBLIC ${FOOYIN_REQUIRED_CXX_FEATURES})
target_compile_definitions(fooyin_lib PRIVATE QT_USE_QSTRINGBUILDER)
target_compile_options(fooyin_lib PRIVATE ${FOOYIN_COMPILE_OPTIONS})

# ---- Fooyin testing ----

if(BUILD_TESTING)
    find_package(GTest)
    add_subdirectory(tests)
endif()

# ---- Fooyin executable ----

set(SOURCES src/app/main.cpp src/app/commandline.cpp)

qt_add_resources(SOURCES data/icons.qrc)

add_executable(fooyin_exe ${SOURCES})
set_target_properties(
    fooyin_exe
    PROPERTIES
        OUTPUT_NAME fooyin
        BUILD_RPATH
        "${FOOYIN_RPATH}/${FOOYIN_RELATIVE_LIB_PATH};${CMAKE_BUILD_RPATH}"
        INSTALL_RPATH
        "${FOOYIN_RPATH}/${FOOYIN_RELATIVE_LIB_PATH};${CMAKE_INSTALL_RPATH}"
        RUNTIME_OUTPUT_DIRECTORY ${FOOYIN_BINARY_OUTPUT_DIR}
)
target_compile_features(fooyin_exe PUBLIC ${FOOYIN_REQUIRED_CXX_FEATURES})
target_compile_options(fooyin_exe PRIVATE ${FOOYIN_COMPILE_OPTIONS})
target_link_libraries(
    fooyin_exe
    PRIVATE KDAB::kdsingleapplication
            fooyin_version
            fooyin_lib
            Fooyin::CorePrivate
)

# ---- Fooyin install ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
    include(FooyinInstall)
endif()

# ---- Fooyin uninstall ----

configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/FooyinUninstall.cmake.in"
    "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake" @ONLY
)
add_custom_target(
    uninstall "${CMAKE_COMMAND}" -P
              "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
)

include(FooyinSummary)
fooyin_print_summary()
feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
