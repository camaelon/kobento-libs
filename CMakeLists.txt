# kobento-libs — the RemoteCompose C++ engine libraries.
#
# Produces:
#   - lib/rccore/librccore.a            engine
#   - lib/rcskia/librcskia.a            Skia bridge
#   - lib/rcplayer/librcplayer.a        desktop player runtime (windowed playback)
#   - tests/                            the engine's tests (RCX_BUILD_TESTS)
#
# Skia, GLFW and miniz are pulled at configure time via CMake FetchContent — nothing is
# vendored. The tools (rc2image, rc2json, …) live in kobento-tools, the desktop viewer in
# kobento-desktop and the iOS build in kobento-ios; each of those adds this tree with
# add_subdirectory (KOBENTO_LIBS_DIR, default: the sibling checkout ../kobento-libs).

cmake_minimum_required(VERSION 3.20)
project(RcX VERSION 0.1.0 LANGUAGES C CXX)

# An empty CMAKE_BUILD_TYPE builds with no optimisation flags at all — a 45 MB document
# took a second to load, decoders ran an order of magnitude slow. Default to Release
# unless the generator is multi-config or the user asked for something else.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# A project that embeds this tree (add_subdirectory) usually wants the libraries only.
# Tests are on when this is the top-level project, off when it is embedded.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    option(RCX_BUILD_TESTS "Build the test suite" ON)
else()
    option(RCX_BUILD_TESTS "Build the test suite" OFF)
endif()

if(APPLE)
    enable_language(OBJCXX)
elseif(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR
        "Desktop build currently targets macOS and Linux. "
        "iOS lib build: see kobento-ios (ios/CMakeLists.txt).")
endif()

include(FetchContent)

# ── Skia (pre-built m143) ─────────────────────────────────────────────
set(SKIA_VERSION "m143-da51f0d60e-2")
set(SKIA_BUILD_TYPE "Release")

if(APPLE)
    set(SKIA_PLATFORM "macos")
    set(SKIA_ARCH "arm64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(SKIA_PLATFORM "linux")
    set(SKIA_ARCH "arm64")
else()
    set(SKIA_PLATFORM "linux")
    set(SKIA_ARCH "x64")
endif()

set(SKIA_ZIP "Skia-${SKIA_VERSION}-${SKIA_PLATFORM}-${SKIA_BUILD_TYPE}-${SKIA_ARCH}.zip")

FetchContent_Declare(skia
    URL "https://github.com/HumbleUI/SkiaBuild/releases/download/${SKIA_VERSION}/${SKIA_ZIP}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_GetProperties(skia)
if(NOT skia_POPULATED)
    FetchContent_Populate(skia)
endif()
set(SKIA_DIR "${skia_SOURCE_DIR}")
if(EXISTS "${SKIA_DIR}/out/${SKIA_BUILD_TYPE}-${SKIA_ARCH}")
    set(SKIA_LIB_DIR "${SKIA_DIR}/out/${SKIA_BUILD_TYPE}-${SKIA_ARCH}")
elseif(EXISTS "${SKIA_DIR}/out/${SKIA_BUILD_TYPE}")
    set(SKIA_LIB_DIR "${SKIA_DIR}/out/${SKIA_BUILD_TYPE}")
else()
    set(SKIA_LIB_DIR "${SKIA_DIR}/out")
endif()
message(STATUS "Skia dir: ${SKIA_DIR}")
message(STATUS "Skia lib dir: ${SKIA_LIB_DIR}")

add_library(skia_lib STATIC IMPORTED GLOBAL)
set_target_properties(skia_lib PROPERTIES
    IMPORTED_LOCATION "${SKIA_LIB_DIR}/libskia.a"
)
target_include_directories(skia_lib INTERFACE "${SKIA_DIR}")

if(APPLE)
    find_library(APPKIT_FW       AppKit)
    find_library(APPSERVICES_FW  ApplicationServices)
    find_library(CF_FW           CoreFoundation)
    find_library(CG_FW           CoreGraphics)
    find_library(CT_FW           CoreText)
    find_library(IMAGEIO_FW      ImageIO)
    find_library(METAL_FW        Metal)
    find_library(FOUNDATION_FW   Foundation)
    find_library(OPENGL_FW       OpenGL)

    set_property(TARGET skia_lib APPEND PROPERTY INTERFACE_LINK_LIBRARIES
        ${APPKIT_FW} ${APPSERVICES_FW} ${CF_FW} ${CG_FW} ${CT_FW}
        ${IMAGEIO_FW} ${METAL_FW} ${FOUNDATION_FW} ${OPENGL_FW}
    )
else()
    find_package(Threads REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SKIA_LINUX_DEPS REQUIRED IMPORTED_TARGET
        fontconfig freetype2 x11 xext xrender gl)
    set_property(TARGET skia_lib APPEND PROPERTY INTERFACE_LINK_LIBRARIES
        PkgConfig::SKIA_LINUX_DEPS Threads::Threads ${CMAKE_DL_LIBS})
endif()

# ── GLFW (used by the viewer app) ────────────────────────────────────
if(APPLE)
    set(GLFW_ZIP "glfw-3.4.bin.MACOS.zip")
    FetchContent_Declare(glfw
        URL "https://github.com/glfw/glfw/releases/download/3.4/${GLFW_ZIP}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_GetProperties(glfw)
    if(NOT glfw_POPULATED)
        FetchContent_Populate(glfw)
    endif()
    set(GLFW_DIR "${glfw_SOURCE_DIR}")
    set(GLFW_LIB_PATH     "${GLFW_DIR}/lib-arm64/libglfw3.a")
    set(GLFW_INCLUDE_PATH "${GLFW_DIR}/include")
    message(STATUS "GLFW dir: ${GLFW_DIR}")
else()
    pkg_check_modules(GLFW REQUIRED IMPORTED_TARGET glfw3)
endif()

# ── miniz (zip support inside the viewer) ─────────────────────────────
FetchContent_Declare(miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG        3.0.2
)
FetchContent_GetProperties(miniz)
if(NOT miniz_POPULATED)
    FetchContent_Populate(miniz)
endif()
set(MINIZ_DIR "${miniz_SOURCE_DIR}")
add_library(miniz STATIC
    "${MINIZ_DIR}/miniz.c"
    "${MINIZ_DIR}/miniz_tdef.c"
    "${MINIZ_DIR}/miniz_tinfl.c"
    "${MINIZ_DIR}/miniz_zip.c"
)
target_include_directories(miniz PUBLIC "${MINIZ_DIR}")
target_compile_definitions(miniz PUBLIC MINIZ_EXPORT=)
file(WRITE "${MINIZ_DIR}/miniz_export.h"
    "#ifndef MINIZ_EXPORT_H\n#define MINIZ_EXPORT_H\n#define MINIZ_EXPORT\n#endif\n")

# ── Sub-projects ───────────────────────────────────────────────────────
add_subdirectory(lib/rccore)
add_subdirectory(lib/rcskia)
add_subdirectory(lib/rcplayer)

if(RCX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
