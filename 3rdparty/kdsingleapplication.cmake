find_package(KDSingleApplication-qt6 QUIET)
if(TARGET KDAB::kdsingleapplication)
    message(STATUS "Using system KDSingleApplication")
    return()
endif()

message(STATUS "Using 3rd-party KDSingleApplication")

set(SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/kdsingleapplication/src/kdsingleapplication.h"
    "${CMAKE_CURRENT_LIST_DIR}/kdsingleapplication/src/kdsingleapplication.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/kdsingleapplication/src/kdsingleapplication_localsocket_p.h"
    "${CMAKE_CURRENT_LIST_DIR}/kdsingleapplication/src/kdsingleapplication_localsocket.cpp"
)

add_library(kdsingleapplication STATIC ${SOURCES})
add_library(KDAB::kdsingleapplication ALIAS kdsingleapplication)

target_compile_definitions(
    kdsingleapplication PRIVATE -DKDSINGLEAPPLICATION_STATIC_BUILD
)

target_include_directories(
    kdsingleapplication PUBLIC "${CMAKE_CURRENT_LIST_DIR}/kdsingleapplication/src"
)

target_link_libraries(
    kdsingleapplication
    PUBLIC Qt6::Core
    PRIVATE Qt6::Network
)
