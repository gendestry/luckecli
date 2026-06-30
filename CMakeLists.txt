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

# CONFIGURE_DEPENDS makes the build re-glob when files are added/moved/removed,
# so the source list never goes stale (e.g. after a restructure) without a
# manual reconfigure. Costs a cheap glob check on each build.
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE utils nlohmann_json ftxui MvrGdtf)
target_include_directories(${PROJECT_NAME}
        PUBLIC
        include
        PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/lib/libMVRgdtf/src/Include
)
