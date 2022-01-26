aux_source_directory(src MHDSRC)
find_package(Threads)
find_package(Sanitizers)

add_code_coverage_all_targets(ALL)

include(CMakeDependentOption)

cmake_dependent_option(
    ENABLE_FUZZER "Enable Fuzzer" ON
    "BUILD_TESTS;BUILD_FUZZ;SANITIZE_ADDRESS;BUILD_STATIC;COMPILER_SUPPORT_FUZZER" OFF)

if (ENABLE_FUZZER)
    message(STATUS "Fuzzer Testing is enabled")
endif ()

if (BUILD_STATIC)
    add_library(staticlib STATIC ${MHDSRC})
    set_target_properties(staticlib PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
    set(LIBS ${LIBS} staticlib)

    if (ENABLE_FUZZER)
        target_compile_options(staticlib PUBLIC -fsanitize=fuzzer-no-link -gline-tables-only
                                                -fsanitize-address-use-after-scope)

        target_link_libraries(staticlib PUBLIC -gline-tables-only -fsanitize=fuzzer-no-link
                                               -fsanitize-address-use-after-scope)
    endif ()

    add_library(cppmhd::lib ALIAS staticlib)
endif ()

if (BUILD_SHARED)
    add_library(sharedlib SHARED ${MHDSRC})
    set_target_properties(sharedlib PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
    set(LIBS ${LIBS} sharedlib)

    if (NOT TARGET cppmhd::lib)
        add_library(cppmhd::lib ALIAS sharedlib)
    endif ()
endif ()

if (NOT TARGET cppmhd::lib)
    message(FATAL_ERROR "BUILD_SHARED and BUILD_STATIC must set at least one")
endif ()

foreach (T IN LISTS LIBS)
    add_sanitizers(${T})
    target_include_directories(${T} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_BINARY_DIR})
    target_compile_options(${T} PUBLIC "$<$<CXX_COMPILER_ID:MSVC>:/D_CRT_NONSTDC_NO_WARNINGS>")
    target_link_libraries(${T} PUBLIC MHD::MHD PCRE2::8 fmt::fmt Threads::Threads
                                      "$<$<TARGET_EXISTS:CARES::CARES>:CARES::CARES>")
endforeach ()

set(SRC_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

if (LOCAL_DOMAIN)
    string(LENGTH ${LOCAL_DOMAIN} DOMAIN_LENGTH)
    set(LOCAL_DOMAIN_LENGTH ${DOMAIN_LENGTH})
endif ()

option(BUILD_TESTS "Build unittest" OFF)

if (BUILD_TESTS)
    find_package(CURL)
    find_package(OpenSSL QUIET COMPONENTS Crypto)
    set(TEST_ENABLE_OPENSSL ${OpenSSL_FOUND})
    set(TEST_ENABLE_CURL ${CURL_FOUND})
    add_subdirectory(test)
endif ()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/config.h.in ${CMAKE_BINARY_DIR}/config.h @ONLY)

put_all_targets_into_coverage(EXCLUDES test/*)
