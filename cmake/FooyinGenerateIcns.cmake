cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED ICONSET_DIR OR NOT DEFINED OUTPUT_ICNS OR NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "ICONSET_DIR, OUTPUT_ICNS, and SOURCE_DIR must be set")
endif()

file(REMOVE_RECURSE "${ICONSET_DIR}")
file(MAKE_DIRECTORY "${ICONSET_DIR}")

function(copy_icon source_name dest_name)
    file(COPY "${SOURCE_DIR}/${source_name}" DESTINATION "${ICONSET_DIR}")
    file(RENAME "${ICONSET_DIR}/${source_name}" "${ICONSET_DIR}/${dest_name}")
endfunction()

function(resize_icon source_name dest_name size)
    execute_process(
        COMMAND sips -z "${size}" "${size}" "${SOURCE_DIR}/${source_name}" --out "${ICONSET_DIR}/${dest_name}"
        RESULT_VARIABLE resize_result
        OUTPUT_QUIET
        ERROR_QUIET
    )
    if(NOT resize_result EQUAL 0)
        message(FATAL_ERROR "Failed to generate ${dest_name}")
    endif()
endfunction()

copy_icon("16-fooyin.png" "icon_16x16.png")
copy_icon("32-fooyin.png" "icon_16x16@2x.png")
copy_icon("32-fooyin.png" "icon_32x32.png")
copy_icon("64-fooyin.png" "icon_32x32@2x.png")
copy_icon("128-fooyin.png" "icon_128x128.png")
copy_icon("256-fooyin.png" "icon_128x128@2x.png")
copy_icon("256-fooyin.png" "icon_256x256.png")
copy_icon("512-fooyin.png" "icon_256x256@2x.png")
copy_icon("512-fooyin.png" "icon_512x512.png")
resize_icon("512-fooyin.png" "icon_512x512@2x.png" 1024)

execute_process(
    COMMAND iconutil -c icns "${ICONSET_DIR}" -o "${OUTPUT_ICNS}"
    RESULT_VARIABLE iconutil_result
    OUTPUT_QUIET
    ERROR_QUIET
)
if(NOT iconutil_result EQUAL 0)
    message(FATAL_ERROR "Failed to generate ${OUTPUT_ICNS}")
endif()
