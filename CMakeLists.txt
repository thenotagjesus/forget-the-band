cmake_minimum_required(VERSION 3.22)

project(CentrophySession VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optional Windows ASIO (requires Steinberg ASIO SDK, not redistributed). Off by default.
# WASAPI remains available on Windows regardless of this option.
option(JUCE_ASIO "Enable ASIO on Windows if the Steinberg ASIO SDK is present" OFF)
set(JUCE_ASIO_SDK_PATH "" CACHE PATH
    "Path to the Steinberg ASIO SDK (folder containing asio.h, or the SDK root with common/asio.h)")

include(FetchContent)

# Prefer already-fetched JUCE 8.0.8 (do not clone if SOURCE_DIR exists).
# Override: cmake -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
set(_session_juce_src "${CMAKE_CURRENT_SOURCE_DIR}/build/_deps/juce-src")
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    SOURCE_DIR     "${_session_juce_src}"
)

FetchContent_MakeAvailable(juce)

juce_add_gui_app(Session
    COMPANY_NAME "Centrophy"
    PRODUCT_NAME "ForgetTheBand"
    BUNDLE_ID com.centrophy.forgettheband
    VERSION ${PROJECT_VERSION}
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon.png"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Forget The Band needs audio input for your guitar."
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE
)

juce_generate_juce_header(Session)

file(GLOB_RECURSE SESSION_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
)

target_sources(Session PRIVATE ${SESSION_SOURCES})

file(GLOB_RECURSE SESSION_SAMPLES
    "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Samples/*.wav")
if(SESSION_SAMPLES)
    juce_add_binary_data(SessionSamples
        HEADER_NAME SessionSamples.h
        NAMESPACE SessionSamples
        SOURCES ${SESSION_SAMPLES})
    set_target_properties(SessionSamples PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

target_include_directories(Session
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Source"
)

target_compile_definitions(Session
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Session,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Session,JUCE_VERSION>"
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_VST=0
        JUCE_PLUGINHOST_AU=0
        JUCE_VST3_HOST_CROSS_PLATFORM_UID=1
)

# ---------------------------------------------------------------------------
# Windows ASIO: only compile it in when the Steinberg SDK is actually found.
# JUCE looks for asio.h / asiosys.h / iasiodrv.h on the include path.
# ---------------------------------------------------------------------------
set(_session_asio 0)
set(_session_asio_includes "")

if(WIN32 AND JUCE_ASIO)
    if(JUCE_ASIO_SDK_PATH STREQUAL "")
        message(WARNING "JUCE_ASIO=ON but JUCE_ASIO_SDK_PATH is empty; building without ASIO (WASAPI still available)")
    else()
        set(_asio_root "${JUCE_ASIO_SDK_PATH}")
        set(_asio_common "")
        set(_asio_hostpc "")

        if(EXISTS "${_asio_root}/common/asio.h")
            set(_asio_common "${_asio_root}/common")
            if(EXISTS "${_asio_root}/host/pc/iasiodrv.h")
                set(_asio_hostpc "${_asio_root}/host/pc")
            endif()
        elseif(EXISTS "${_asio_root}/asio.h")
            set(_asio_common "${_asio_root}")
            if(EXISTS "${_asio_root}/../host/pc/iasiodrv.h")
                set(_asio_hostpc "${_asio_root}/../host/pc")
            elseif(EXISTS "${_asio_root}/host/pc/iasiodrv.h")
                set(_asio_hostpc "${_asio_root}/host/pc")
            endif()
        endif()

        if(_asio_common STREQUAL "")
            message(WARNING
                "JUCE_ASIO=ON but neither asio.h nor common/asio.h was found under "
                "${JUCE_ASIO_SDK_PATH}; building without ASIO (WASAPI still available)")
        else()
            set(_session_asio 1)
            list(APPEND _session_asio_includes "${_asio_common}")
            if(NOT _asio_hostpc STREQUAL "")
                list(APPEND _session_asio_includes "${_asio_hostpc}")
            endif()
            message(STATUS "ASIO enabled; headers at ${_session_asio_includes}")
        endif()
    endif()
endif()

if(_session_asio)
    target_compile_definitions(Session PRIVATE JUCE_ASIO=1)
    target_include_directories(Session PRIVATE ${_session_asio_includes})
else()
    target_compile_definitions(Session PRIVATE JUCE_ASIO=0)
endif()

target_link_libraries(Session
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(TARGET SessionSamples)
    target_link_libraries(Session PRIVATE SessionSamples)
endif()

if(MSVC)
    target_compile_options(Session PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(Session PRIVATE -Wall -Wextra)
endif()

# Dual-pipeline tracker: aubio (audio thread) + ONNX Runtime Basic Pitch (worker).
# Each module sets FTB_HAS_AUBIO / FTB_HAS_ONNX to 1 when found, else 0.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/AubioMin.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/OnnxRuntime.cmake")

