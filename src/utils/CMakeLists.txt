fooyin_add_library(Utils
    TYPE SHARED
    DEPENDS
        Qt6::Core
        Qt6::Widgets
    SOURCES
        id.cpp
        paths.cpp
        utils.cpp
)
