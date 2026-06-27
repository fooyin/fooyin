find_package(projectM4 QUIET COMPONENTS Playlist)

if(TARGET libprojectM::projectM AND TARGET libprojectM::playlist)
    message(STATUS "Using system projectM")
    return()
endif()

if(NOT FETCH_PROJECTM)
    message(STATUS "System projectM not found and FETCH_PROJECTM=OFF; not fetching projectM")
    return()
endif()

message(STATUS "Using 3rd-party projectM")

function(setup_projectm)
    include(FetchContent)

    set(fooyin_build_shared_libs ${BUILD_SHARED_LIBS})

    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_TESTING OFF)
    set(BUILD_DOCS OFF)
    set(ENABLE_INSTALL OFF)
    set(ENABLE_PLAYLIST ON)
    set(ENABLE_SDL_UI OFF)
    set(ENABLE_SYSTEM_PROJECTM_EVAL OFF)

    FetchContent_Declare(
        projectm
        GIT_REPOSITORY https://github.com/projectM-visualizer/projectm.git
        GIT_TAG v4.1.6
        GIT_SHALLOW TRUE
        GIT_SUBMODULES_RECURSE TRUE
        EXCLUDE_FROM_ALL
    )

    FetchContent_MakeAvailable(projectm)

    set(BUILD_SHARED_LIBS
        ${fooyin_build_shared_libs}
        CACHE BOOL "Build fooyin libraries as shared" FORCE
    )
endfunction()

setup_projectm()
