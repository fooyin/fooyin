find_package(QCoro6 0.9.0 QUIET COMPONENTS Core)
if(TARGET QCoro6::Core)
    message(STATUS "Using system QCoro")
else()
    message(STATUS "Using 3rd-party QCoro")

    include(FetchContent)
    FetchContent_Declare(
        qcoro
        GIT_REPOSITORY https://github.com/danvratil/qcoro.git
        GIT_TAG v0.9.0
    )
    FetchContent_GetProperties(qcoro)
    if(NOT qcoro_POPULATED)
        FetchContent_Populate(qcoro)
        set(qcoro_BUILD_SHARED_LIBS OFF)
        set(qcoro_BUILD_TESTING OFF)
        set(QCORO_BUILD_EXAMPLES OFF)
        set(QCORO_WITH_QTWEBSOCKETS OFF)
        set(QCORO_WITH_QTQUICK OFF)
        set(QCORO_WITH_QTDBUS OFF)
        set(QCORO_DISABLE_DEPRECATED_TASK_H ON)
        set(QCORO_WITH_QML OFF)

        # Disable feature summary output
        set(log_level ${CMAKE_MESSAGE_LOG_LEVEL})
        set(CMAKE_MESSAGE_LOG_LEVEL NOTICE)
    endif()
    add_subdirectory(${qcoro_SOURCE_DIR} ${qcoro_BINARY_DIR} EXCLUDE_FROM_ALL)
    set(CMAKE_MESSAGE_LOG_LEVEL ${log_level})
endif()

qcoro_enable_coroutines()
