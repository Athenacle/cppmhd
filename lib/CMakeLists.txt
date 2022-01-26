aux_source_directory(src MHDSRC)
find_package(Threads)
find_package(Sanitizers)

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
    target_include_directories(${T} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
                                           "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

    target_compile_options(${T} PUBLIC "$<$<CXX_COMPILER_ID:MSVC>:/D_CRT_NONSTDC_NO_WARNINGS>")

    set_target_properties(${T} PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${MAJOR_VERSION})

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

set(project_config_in "${CMAKE_SOURCE_DIR}/cmake/cppmhdConfig.cmake.in")
set(project_config_out "${CMAKE_CURRENT_BINARY_DIR}/cppmhdConfig.cmake")
set(config_targets_file "cppmhdConfigTargets.cmake")
set(version_config_file "${CMAKE_CURRENT_BINARY_DIR}/cppmhdConfigVersion.cmake")
set(export_dest_dir "${CMAKE_INSTALL_LIBDIR}/cmake/cppmhd")

include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

if (BUILD_SHARED)
    install(TARGETS sharedlib EXPORT cppmhd LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif ()

if (BUILD_STATIC)
    install(TARGETS staticlib EXPORT cppmhd LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif ()

install(EXPORT cppmhd DESTINATION ${export_dest_dir} NAMESPACE cppmhd:: FILE ${config_targets_file})
include(CMakePackageConfigHelpers)
configure_package_config_file("${project_config_in}" "${project_config_out}"
                              INSTALL_DESTINATION ${export_dest_dir})

write_basic_package_version_file("${version_config_file}" COMPATIBILITY SameMajorVersion)
install(FILES "${project_config_out}" "${version_config_file}"
              "${CMAKE_SOURCE_DIR}/cmake/FindCARES.cmake" "${CMAKE_SOURCE_DIR}/cmake/FindMHD.cmake"
              "${CMAKE_SOURCE_DIR}/cmake/FindPCRE2.cmake" DESTINATION ${export_dest_dir})
