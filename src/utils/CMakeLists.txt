fooyin_add_library(Utils
    TYPE SHARED
    PREFIX Yes
    DEPENDS
        Qt6::Core
        Qt6::Widgets
    SOURCES
        id.cpp
        paths.cpp
        utils.cpp
)
