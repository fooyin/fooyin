find_package(KDSingleApplication-qt6 QUIET)
if(TARGET KDAB::kdsingleapplication)
    message(STATUS "Using system KDSingleApplication")
    return()
endif()

message(STATUS "Using 3rd-party KDSingleApplication")

set(KDSingleApplication_STATIC ON)
set(KDSingleApplication_QT6 ON)
set(KDSingleApplication_EXAMPLES OFF)

include(FetchContent)
FetchContent_Declare(
    kdsingleapplication
    GIT_REPOSITORY https://www.github.com/KDAB/KDSingleApplication.git
    GIT_TAG v1.2.0
)

FetchContent_MakeAvailable(kdsingleapplication)
