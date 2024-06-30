cmake_minimum_required(VERSION 3.18)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

include(CheckBuildDir)
include(CheckLevel)

project(
    fooyin
    VERSION 0.5.1
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
set(CMAKE_LINK_DEPENDS_NO_SHARED ON)

# Enable setting options with SET cmake command
# Needed for some 3rdparty libs
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

if(NOT DEFINED CMAKE_SKIP_BUILD_RPATH)
    set(CMAKE_SKIP_BUILD_RPATH OFF)
endif()
if(NOT DEFINED CMAKE_BUILD_WITH_INSTALL_RPATH)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)
endif()
if(NOT DEFINED CMAKE_INSTALL_RPATH_USE_LINK_PATH)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

set(FOOYIN_REQUIRED_CXX_FEATURES cxx_std_20)

include(FooyinMacrosInternal)

fooyin_option(BUILD_SHARED_LIBS "Build fooyin libraries as shared" ON)
fooyin_option(BUILD_TESTING "Build fooyin tests" OFF)
fooyin_option(BUILD_PLUGINS "Build plugins included with fooyin" ON)
fooyin_option(BUILD_ALSA "Build ALSA plugin" ON)
fooyin_option(BUILD_TRANSLATIONS "Build translation files" ON)
fooyin_option(BUILD_CCACHE "Build using CCache if found" ON)
fooyin_option(BUILD_PCH "Build with precompiled header support" OFF)
fooyin_option(BUILD_WERROR "Build with -Werror" OFF)
fooyin_option(BUILD_ASAN "Enable AddressSanitizer" OFF)
fooyin_option(INSTALL_FHS "Install in Linux distros /usr hierarchy" ON)
fooyin_option(INSTALL_HEADERS "Install public development headers" OFF)

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
if(BUILD_ASAN)
    list(APPEND FOOYIN_COMPILE_OPTIONS -fsanitize=address,undefined)
    list(APPEND FOOYIN_LINK_OPTIONS -fsanitize=address,undefined)
endif()

list(
    APPEND
    FOOYIN_COMPILE_DEFINITIONS
    QT_USE_QSTRINGBUILDER
    QT_DISABLE_DEPRECATED_BEFORE=0x060200
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
    QT_NO_CAST_FROM_BYTEARRAY
    QT_NO_NARROWING_CONVERSIONS_IN_CONNECT
    QT_STRICT_ITERATORS
)

# ---- Dependencies ----

find_package(
    Qt6 REQUIRED
    COMPONENTS Core
               Widgets
               Gui
               Sql
               Concurrent
               Network
               Svg
)
set_package_properties(Qt6Core PROPERTIES TYPE REQUIRED)
set_package_properties(Qt6Widgets PROPERTIES TYPE REQUIRED)
set_package_properties(Qt6Gui PROPERTIES TYPE REQUIRED)
set_package_properties(Qt6Sql PROPERTIES TYPE REQUIRED)
set_package_properties(Qt6Concurrent PROPERTIES TYPE REQUIRED)
set_package_properties(Qt6Network PROPERTIES TYPE REQUIRED)
set_package_properties(Qt6Svg PROPERTIES TYPE REQUIRED)

find_package(ICU COMPONENTS uc i18n REQUIRED)
find_package(Taglib REQUIRED taglib>=1.12)
find_package(
    FFmpeg REQUIRED
    COMPONENTS AVCODEC
               AVFORMAT
               AVUTIL
)

include(3rdparty/3rdparty.cmake)

if(BUILD_CCACHE)
    find_program(CCACHE_EXECUTABLE ccache)
    if(CCACHE_EXECUTABLE)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_EXECUTABLE})
        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE_EXECUTABLE})
    endif()
endif()

# ---- Fooyin paths ----

include(GNUInstallDirs)

set(DATAROOTDIR "${CMAKE_INSTALL_DATAROOTDIR}")
fooyin_convert_to_relative_path(DATAROOTDIR)

if(INSTALL_FHS)
  set(BIN_INSTALL_DIR "${CMAKE_INSTALL_BINDIR}")
  set(LIB_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/fooyin")
  set(TRANSLATION_INSTALL_DIR "${DATAROOTDIR}/fooyin/translations")
else()
  set(BIN_INSTALL_DIR .)
  set(LIB_INSTALL_DIR .)
  set(TRANSLATION_INSTALL_DIR "translations")
endif()

fooyin_convert_to_relative_path(BIN_INSTALL_DIR)
fooyin_convert_to_relative_path(LIB_INSTALL_DIR)

set(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_INCLUDEDIR}/fooyin")
set(CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/fooyin")

set(XDG_APPS_INSTALL_DIR "${DATAROOTDIR}/applications")
set(APPDATA_INSTALL_DIR "${DATAROOTDIR}/metainfo")
set(ICON_INSTALL_DIR "${DATAROOTDIR}/icons")

set(DOC_INSTALL_DIR "${DATAROOTDIR}/doc/fooyin/")
file(RELATIVE_PATH FOOYIN_RELATIVE_TRANSLATION_PATH "/${BIN_INSTALL_DIR}" "/${TRANSLATION_INSTALL_DIR}")

set(FOOYIN_PLUGIN_INSTALL_DIR "${LIB_INSTALL_DIR}/plugins")
file(RELATIVE_PATH FOOYIN_RELATIVE_PLUGIN_PATH "/${BIN_INSTALL_DIR}" "/${FOOYIN_PLUGIN_INSTALL_DIR}")

if(NOT FOOYIN_OUTPUT_PREFIX)
    set(FOOYIN_OUTPUT_PREFIX "${PROJECT_BINARY_DIR}/run")
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${FOOYIN_OUTPUT_PREFIX}/${BIN_INSTALL_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${FOOYIN_OUTPUT_PREFIX}/${LIB_INSTALL_DIR}")
set(FOOYIN_PLUGIN_OUTPUT_DIRECTORY "${FOOYIN_OUTPUT_PREFIX}/${FOOYIN_PLUGIN_INSTALL_DIR}")

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
        fooyin_pch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/app/pch.h
    )
    set_target_properties(
        fooyin_pch
        PROPERTIES POSITION_INDEPENDENT_CODE ON
                   CXX_VISIBILITY_PRESET hidden
                   VISIBILITY_INLINES_HIDDEN YES
    )
    target_link_libraries(fooyin_pch PRIVATE Qt::Core Qt::Gui Qt::Widgets)
    target_compile_features(fooyin_pch PRIVATE ${FOOYIN_REQUIRED_CXX_FEATURES})
    target_compile_definitions(fooyin_pch PRIVATE ${FOOYIN_COMPILE_DEFINITIONS})
    target_compile_options(fooyin_pch PRIVATE ${FOOYIN_COMPILE_OPTIONS})
    target_link_options(fooyin_pch INTERFACE ${FOOYIN_LINK_OPTIONS})
    if(CMAKE_VERSION GREATER_EQUAL 3.19)
        set(CMAKE_PCH_INSTANTIATE_TEMPLATES ON)
    endif()
endif()

configure_file(src/app/version.h.in app/version.h)
add_library(fooyin_version OBJECT)
target_include_directories(
    fooyin_version PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/app>
)
configure_file(src/app/config.h.in app/config.h)
add_library(fooyin_config OBJECT)
target_include_directories(
    fooyin_config PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/app>
)

add_library(fooyin_lib OBJECT)

target_include_directories(
    fooyin_lib PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
                      "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/app>"
)

set_target_properties(
    fooyin_lib PROPERTIES VERSION "${FOOYIN_VERSION}" EXPORT_NAME Fooyin
)

add_subdirectory(src)

target_link_libraries(
    fooyin_lib
    PUBLIC Fooyin::Core
           Fooyin::Gui
           Fooyin::Utils
)

target_compile_features(fooyin_lib PUBLIC ${FOOYIN_REQUIRED_CXX_FEATURES})
target_compile_definitions(fooyin_lib PRIVATE ${FOOYIN_COMPILE_DEFINITIONS})
target_compile_options(fooyin_lib PRIVATE ${FOOYIN_COMPILE_OPTIONS})
target_link_options(fooyin_lib INTERFACE ${FOOYIN_LINK_OPTIONS})

# ---- Fooyin translations ----

if(BUILD_TRANSLATIONS)
    find_package(
        Qt6 REQUIRED
        COMPONENTS LinguistTools
    )

    if(Qt6LinguistTools_FOUND)
        set(translation_dir
            "${PROJECT_SOURCE_DIR}/data/translations"
            CACHE PATH "Path to the *.ts translation files"
        )

        # Detect new .ts files
        set_property(
                DIRECTORY
                APPEND
                PROPERTY CMAKE_CONFIGURE_DEPENDS ${translation_dir}
        )

        file(GLOB_RECURSE ts_files ${translation_dir}/*.ts)
        # Exclude the translation source
        list(REMOVE_ITEM ts_files ${translation_dir}/fooyin.ts)

        set_source_files_properties(
            ${ts_files} PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/data"
        )

        qt_add_lrelease(
            fooyin_lib
            TS_FILES
            ${ts_files}
            NO_GLOBAL_TARGET
            QM_FILES_OUTPUT_VARIABLE
            qm_files
        )

        if(NOT CMAKE_SKIP_INSTALL_RULES)
            install(FILES ${qm_files} DESTINATION ${TRANSLATION_INSTALL_DIR})
        endif()
    endif()
endif()

# ---- Fooyin testing ----

if(BUILD_TESTING)
    find_package(GTest)
    add_subdirectory(tests)
endif()

# ---- Fooyin executable ----

set(SOURCES ${SOURCES} src/app/main.cpp src/app/commandline.cpp)

qt_add_resources(SOURCES data/data.qrc)
qt_add_resources(SOURCES data/icons.qrc)

add_executable(fooyin ${SOURCES} ${TRANSLATIONS})
fooyin_set_rpath(fooyin ${BIN_INSTALL_DIR})
target_compile_features(fooyin PUBLIC ${FOOYIN_REQUIRED_CXX_FEATURES})
target_compile_definitions(fooyin PRIVATE ${FOOYIN_COMPILE_DEFINITIONS})
target_compile_options(fooyin PRIVATE ${FOOYIN_COMPILE_OPTIONS})
target_link_options(fooyin INTERFACE ${FOOYIN_LINK_OPTIONS})
target_link_libraries(
    fooyin
    PRIVATE KDAB::kdsingleapplication
            Qt6::Network
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
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FooyinUninstall.cmake.in"
    "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake" @ONLY
)
add_custom_target(
    uninstall "${CMAKE_COMMAND}" -P
              "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
)

include(FooyinPackaging)

include(FooyinSummary)
fooyin_print_summary()
