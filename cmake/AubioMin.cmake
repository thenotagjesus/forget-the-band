# Minimal aubio (no waf). Onset + pitch + built-in Ooura FFT.
# Do NOT #define HAVE_FFTW3 0 — aubio_priv.h tests defined(HAVE_FFTW3), so a 0 still includes fftw3.h.
include(FetchContent)
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

set(_aubio_src "${CMAKE_CURRENT_SOURCE_DIR}/build/_deps/aubio-src")
option(FTB_ENABLE_AUBIO "Build aubio (onset/pitch) into Session" ON)

if(NOT FTB_ENABLE_AUBIO)
    target_compile_definitions(Session PRIVATE FTB_HAS_AUBIO=0)
    return()
endif()

if(NOT EXISTS "${_aubio_src}/src/aubio.h")
    FetchContent_Declare(aubio
        URL https://github.com/aubio/aubio/archive/refs/tags/0.4.9.tar.gz
        SOURCE_DIR "${_aubio_src}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_GetProperties(aubio)
    if(NOT aubio_POPULATED)
        message(STATUS "Fetching aubio 0.4.9 (no waf; compiling a subset of C sources)")
        FetchContent_Populate(aubio)
    endif()
    set(_aubio_src "${aubio_SOURCE_DIR}")
endif()

if(NOT EXISTS "${_aubio_src}/src/aubio.h")
    message(WARNING "aubio sources missing; building without FTB_HAS_AUBIO")
    target_compile_definitions(Session PRIVATE FTB_HAS_AUBIO=0)
    return()
endif()

set(_aubio_cfg "${CMAKE_CURRENT_BINARY_DIR}/aubio_config")
file(MAKE_DIRECTORY "${_aubio_cfg}")
file(WRITE "${_aubio_cfg}/config.h" [=[
#ifndef AUBIO_CONFIG_H
#define AUBIO_CONFIG_H
#define HAVE_STDLIB_H 1
#define HAVE_STDIO_H 1
#define HAVE_MATH_H 1
#define HAVE_STRING_H 1
#define HAVE_ERRNO_H 1
#define HAVE_LIMITS_H 1
#define HAVE_STDARG_H 1
#define HAVE_C99_VARARGS_MACROS 1
#ifndef _MSC_VER
#define HAVE_COMPLEX_H 1
#endif
#ifdef _MSC_VER
#define strdup _strdup
#endif
/* HAVE_FFTW3 / HAVE_ACCELERATE / HAVE_INTEL_IPP must stay *undefined* (Ooura FFT). */
#define HAVE_WAV 0
#define HAVE_AUBIO_DOUBLE 0
#endif
]=])

file(GLOB_RECURSE AUBIO_C CONFIGURE_DEPENDS "${_aubio_src}/src/*.c")
list(FILTER AUBIO_C EXCLUDE REGEX ".*/(dct_fftw|dct_accelerate|dct_ipp|windll|source_sndfile|source_avcodec|source_apple_audio|sink_sndfile|sink_apple_audio|audio_unit|source_wavread|sink_wavwrite|utils_apple_audio)\\.c$")

add_library(aubio STATIC ${AUBIO_C})
set_target_properties(aubio PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
target_include_directories(aubio
    PUBLIC "${_aubio_src}/src"
    PRIVATE "${_aubio_cfg}"
)
target_compile_definitions(aubio
    PRIVATE
        HAVE_CONFIG_H
        HAVE_WAV=0
        HAVE_AUBIO_DOUBLE=0
        $<$<BOOL:${MSVC}>:_CRT_SECURE_NO_WARNINGS>
        $<$<BOOL:${MSVC}>:_USE_MATH_DEFINES>
)
if(MSVC)
    target_compile_options(aubio PRIVATE /W3 /wd4244 /wd4267 /wd4018 /wd4996 /wd4305 /wd4245)
else()
    target_compile_options(aubio PRIVATE -w)
    target_link_libraries(aubio PUBLIC m)
endif()

target_link_libraries(Session PRIVATE aubio)
target_compile_definitions(Session PRIVATE FTB_HAS_AUBIO=1)
message(STATUS "aubio enabled (${_aubio_src})")
