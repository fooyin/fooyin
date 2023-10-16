cmake_minimum_required(VERSION 3.12)

set(HEADERS kdsingleapplication.h kdsingleapplication_localsocket_p.h)
set(SOURCES kdsingleapplication.cpp kdsingleapplication_localsocket.cpp)

add_library(kdsingleapplication STATIC ${HEADERS} ${SOURCES})
add_library(KDAB::kdsingleapplication ALIAS kdsingleapplication)

target_include_directories(
    kdsingleapplication PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(kdsingleapplication PUBLIC Qt6::Core Qt6::Network)

target_compile_definitions(
    kdsingleapplication PRIVATE -DKDSINGLEAPPLICATION_STATIC_BUILD
)
