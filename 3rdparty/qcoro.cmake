# find_package(QCoro6 0.9.0 QUIET COMPONENTS Core Network)

if(TARGET QCoro6::Core)
    message(STATUS "Using system QCoro")
else()
    message(STATUS "Using 3rd-party QCoro")

    function(setup_qcoro)
        include(FetchContent)

        set(BUILD_SHARED_LIBS OFF)
        set(BUILD_TESTING OFF)
        set(CMAKE_POSITION_INDEPENDENT_CODE ON)
        set(QCORO_BUILD_EXAMPLES OFF)
        set(QCORO_WITH_QTWEBSOCKETS OFF)
        set(QCORO_WITH_QTQUICK OFF)
        set(QCORO_WITH_QTDBUS OFF)
        set(QCORO_DISABLE_DEPRECATED_TASK_H ON)
        set(QCORO_WITH_QML OFF)

        FetchContent_Declare(
            qcoro
            GIT_REPOSITORY https://github.com/qcoro/qcoro.git
            GIT_TAG v0.12.0
            SOURCE_SUBDIR "NeedManualAddSubDir"
        )
        FetchContent_MakeAvailable(qcoro)

        set(log_level ${CMAKE_MESSAGE_LOG_LEVEL})
        if (NOT VERBOSE_FETCH)
          set(CMAKE_MESSAGE_LOG_LEVEL NOTICE)
        endif()

        add_subdirectory(${qcoro_SOURCE_DIR} ${qcoro_BINARY_DIR} EXCLUDE_FROM_ALL)

        set(CMAKE_MESSAGE_LOG_LEVEL $log_level)
    endfunction()

    setup_qcoro()
endif()

qcoro_enable_coroutines()
