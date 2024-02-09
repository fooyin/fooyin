find_package(KDSingleApplication-qt6 QUIET)
if(TARGET KDAB::kdsingleapplication)
    message(STATUS "Using system KDSingleApplication")
    return()
endif()

message(STATUS "Using 3rd-party KDSingleApplication")

include(FetchContent)
FetchContent_Declare(
    kdsingleapplication
    GIT_REPOSITORY https://github.com/KDAB/KDSingleApplication.git
    GIT_TAG v1.1.0
)
FetchContent_GetProperties(kdsingleapplication)
if(NOT kdsingleapplication_POPULATED)
    set(KDSingleApplication_STATIC ON)
    set(KDSingleApplication_QT6 ON)
    set(KDSingleApplication_EXAMPLES OFF)
    FetchContent_Populate(kdsingleapplication)
    add_subdirectory(
        ${kdsingleapplication_SOURCE_DIR} ${kdsingleapplication_BINARY_DIR}
        EXCLUDE_FROM_ALL
    )
endif()
