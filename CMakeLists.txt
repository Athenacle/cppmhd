cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME cppmhd)

project(${PROJECT_NAME})

set(MAJOR_VERSION 0)
set(MINOR_VERSION 0)
set(PATCH_VERSION 1)
set(PROJECT_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}")

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/)
set(CMAKE_CXX_STANDARD 11)

include(CheckCompiler)
include(Coverage)
include(Dependency)

option(BUILD_SHARED "Shared" ON)
option(BUILD_STATIC "Static" ON)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif ()

add_subdirectory(lib)
add_subdirectory(examples)

add_code_coverage_all_targets(ALL)
put_all_targets_into_coverage(EXCLUDES test/*)