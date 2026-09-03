# Prebuilt ONNX Runtime. Option FTB_ONNXRUNTIME_ROOT, else download the official zip.
set(FTB_ONNXRUNTIME_ROOT "" CACHE PATH "Root of a prebuilt onnxruntime (include/ + lib/)")
option(FTB_ENABLE_ONNX "Link ONNX Runtime for Basic Pitch" ON)

set(_ort_ver "1.19.2")

if(NOT FTB_ENABLE_ONNX)
    target_compile_definitions(Session PRIVATE FTB_HAS_ONNX=0)
    return()
endif()

set(_ort_root "${FTB_ONNXRUNTIME_ROOT}")

if(_ort_root STREQUAL "")
    set(_ort_store "${CMAKE_CURRENT_SOURCE_DIR}/build/_deps/onnxruntime")
    if(WIN32)
        set(_ort_name "onnxruntime-win-x64-${_ort_ver}")
        set(_ort_url "https://github.com/microsoft/onnxruntime/releases/download/v${_ort_ver}/${_ort_name}.zip")
        set(_ort_archive "${_ort_store}/${_ort_name}.zip")
    elseif(UNIX AND NOT APPLE)
        set(_ort_name "onnxruntime-linux-x64-${_ort_ver}")
        set(_ort_url "https://github.com/microsoft/onnxruntime/releases/download/v${_ort_ver}/${_ort_name}.tgz")
        set(_ort_archive "${_ort_store}/${_ort_name}.tgz")
    else()
        set(_ort_name "")
        set(_ort_url "")
    endif()

    if(NOT _ort_name STREQUAL "")
        set(_ort_extract "${_ort_store}/${_ort_name}")
        if(NOT EXISTS "${_ort_extract}/include")
            file(MAKE_DIRECTORY "${_ort_store}")
            if(NOT EXISTS "${_ort_archive}")
                message(STATUS "Downloading ONNX Runtime ${_ort_ver}")
                file(DOWNLOAD "${_ort_url}" "${_ort_archive}" SHOW_PROGRESS STATUS _dl)
                list(GET _dl 0 _dl_code)
                if(NOT _dl_code EQUAL 0)
                    message(WARNING "ONNX Runtime download failed; building without FTB_HAS_ONNX")
                    target_compile_definitions(Session PRIVATE FTB_HAS_ONNX=0)
                    return()
                endif()
            endif()
            file(ARCHIVE_EXTRACT INPUT "${_ort_archive}" DESTINATION "${_ort_store}")
        endif()
        set(_ort_root "${_ort_extract}")
    endif()
endif()

set(_ort_inc "")
if(EXISTS "${_ort_root}/include/onnxruntime_cxx_api.h")
    set(_ort_inc "${_ort_root}/include")
elseif(EXISTS "${_ort_root}/include/onnxruntime/core/session/onnxruntime_cxx_api.h")
    set(_ort_inc "${_ort_root}/include/onnxruntime/core/session")
endif()

set(_ort_lib "")
if(WIN32)
    if(EXISTS "${_ort_root}/lib/onnxruntime.lib")
        set(_ort_lib "${_ort_root}/lib/onnxruntime.lib")
    endif()
else()
    if(EXISTS "${_ort_root}/lib/libonnxruntime.so")
        set(_ort_lib "${_ort_root}/lib/libonnxruntime.so")
    elseif(EXISTS "${_ort_root}/lib/libonnxruntime.dylib")
        set(_ort_lib "${_ort_root}/lib/libonnxruntime.dylib")
    endif()
endif()

set(_ort_dll "")
if(EXISTS "${_ort_root}/lib/onnxruntime.dll")
    set(_ort_dll "${_ort_root}/lib/onnxruntime.dll")
elseif(EXISTS "${_ort_root}/bin/onnxruntime.dll")
    set(_ort_dll "${_ort_root}/bin/onnxruntime.dll")
endif()

if(_ort_inc STREQUAL "" OR _ort_lib STREQUAL "")
    message(WARNING "ONNX Runtime not found (set FTB_ONNXRUNTIME_ROOT); building without FTB_HAS_ONNX")
    target_compile_definitions(Session PRIVATE FTB_HAS_ONNX=0)
    return()
endif()

target_include_directories(Session PRIVATE "${_ort_inc}")
target_link_libraries(Session PRIVATE "${_ort_lib}")
target_compile_definitions(Session PRIVATE FTB_HAS_ONNX=1)
message(STATUS "ONNX Runtime enabled (${_ort_root})")

set(_model "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Models/basic_pitch.onnx")

if(WIN32 AND NOT _ort_dll STREQUAL "")
    add_custom_command(TARGET Session POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_ort_dll}" "$<TARGET_FILE_DIR:Session>/onnxruntime.dll"
        COMMENT "Copy onnxruntime.dll next to ForgetTheBand.exe")
endif()

if(EXISTS "${_model}")
    add_custom_command(TARGET Session POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_model}" "$<TARGET_FILE_DIR:Session>/basic_pitch.onnx"
        COMMENT "Copy basic_pitch.onnx next to the binary")
endif()
