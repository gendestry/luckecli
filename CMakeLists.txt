cmake_minimum_required(VERSION 3.10)

# Project setup
project(luckecli VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Use ccache as the compiler launcher when available, so clean/from-scratch
# rebuilds of the rarely-changing submodule libs come from cache. Silently
# falls back to the plain compiler if ccache isn't installed.
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

# Emit all built libraries (static .a and shared .so) into lib_bin/ at the
# project root. Set before add_subdirectory so the submodule targets inherit it.
# set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib_bin)
# set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib_bin)

add_subdirectory(lib/json)
add_subdirectory(lib/Utils)
add_subdirectory(lib/FTXUI)

# libMVRgdtf (GDTF/MVR read+write). Build the static lib with its bundled tinyXML
# instead of xerces, and without MVR-xchange (which would require boost), so the
# only extra link dep is libuuid. The target it exposes is `MvrGdtf`.
set(DONT_USE_XERCES_AS_XMLLIB TRUE CACHE BOOL "" FORCE)
set(BUILD_MVR_XCHANGE FALSE CACHE BOOL "" FORCE)
set(UNITTEST FALSE CACHE BOOL "" FORCE)
add_subdirectory(lib/libMVRgdtf)

# ── Engine library ─────────────────────────────────────────────────────────
# The reusable, frontend-agnostic core: domain model, command engine, device
# networking, GDTF export, and support utilities. No ftxui dependency — the
# only UI it knows about is the abstract ui::Display interface (header-only).
# CONFIGURE_DEPENDS re-globs when files are added/moved/removed, so the source
# list never goes stale after a restructure without a manual reconfigure.
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
        src/core/*.cpp
        src/support/*.cpp)

add_library(lucke_core STATIC ${CORE_SOURCES})
target_link_libraries(lucke_core PUBLIC utils nlohmann_json MvrGdtf)
target_include_directories(lucke_core
        PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        PRIVATE
        ${PROJECT_SOURCE_DIR}/lib/libMVRgdtf/src/Include
)

# ── CLI / TUI application ────────────────────────────────────────────────────
# The frontend built on top of the engine: concrete displays and TUI
# components (ftxui) plus the entry point.
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
        src/ui/*.cpp
        src/main.cpp)

add_executable(${PROJECT_NAME} ${APP_SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE lucke_core ftxui)
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/src)
