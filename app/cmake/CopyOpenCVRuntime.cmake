if(CONFIG STREQUAL "Debug" AND OPENCV_DEBUG_DLL AND NOT OPENCV_DEBUG_DLL MATCHES "-NOTFOUND$")
    set(_opencv_runtime_dll "${OPENCV_DEBUG_DLL}")
else()
    set(_opencv_runtime_dll "${OPENCV_RELEASE_DLL}")
endif()

if(NOT _opencv_runtime_dll OR NOT EXISTS "${_opencv_runtime_dll}")
    message(FATAL_ERROR "OpenCV runtime DLL is unavailable: ${_opencv_runtime_dll}")
endif()
if(NOT TARGET_FILE_DIR OR NOT IS_DIRECTORY "${TARGET_FILE_DIR}")
    message(FATAL_ERROR "Target output directory is unavailable: ${TARGET_FILE_DIR}")
endif()

get_filename_component(_opencv_runtime_name "${_opencv_runtime_dll}" NAME)
file(COPY_FILE
    "${_opencv_runtime_dll}"
    "${TARGET_FILE_DIR}/${_opencv_runtime_name}"
    ONLY_IF_DIFFERENT
)
