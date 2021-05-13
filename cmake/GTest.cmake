find_package(Threads REQUIRED)
include(ExternalProject)

set(GTEST_ROOT ${CMAKE_CURRENT_BINARY_DIR}/gtest/root CACHE FILEPATH "")

ExternalProject_Add(
    gtest
    URL https://github.com/google/googletest/archive/release-1.10.0.zip
    URL_HASH SHA256=94c634d499558a76fa649edb13721dce6e98fb1e7018dfaeba3cd7a083945e91
    DOWNLOAD_NO_PROGRESS ON
    BUILD_BYPRODUCTS
        ${GTEST_ROOT}/src/gtest-build/lib/libgmock.a ${GTEST_ROOT}/src/gtest-build/lib/libgtest.a
        ${GTEST_ROOT}/src/gtest-build/lib/libgtest_main.a
        ${GTEST_ROOT}/src/gtest-build/lib/libgmock_main.a
    PREFIX ${GTEST_ROOT}
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release
    LOG_INSTALL OFF
    INSTALL_COMMAND "")

ExternalProject_Get_Property(gtest source_dir binary_dir)

if (${COMPILER_SUPPORT_NO_ZERO_AS_NULL})
    add_compile_options(-Wno-zero-as-null-pointer-constant)
endif ()

file(MAKE_DIRECTORY ${source_dir}/googletest/include)
file(MAKE_DIRECTORY ${source_dir}/googlemock/include)

add_library(GTest::GTest STATIC IMPORTED)
set_target_properties(
    GTest::GTest
    PROPERTIES INTERFACE_INCLUDE_DIRECTORIES $<BUILD_INTERFACE:${source_dir}/googletest/include>
               IMPORTED_LOCATION ${binary_dir}/lib/libgtest.a)
add_dependencies(GTest::GTest gtest)

add_library(GTest::GTestMain STATIC IMPORTED)
set_target_properties(
    GTest::GTestMain
    PROPERTIES INTERFACE_INCLUDE_DIRECTORIES $<BUILD_INTERFACE:${source_dir}/googletest/include>
               IMPORTED_LOCATION ${binary_dir}/lib/libgtest_main.a)
add_dependencies(GTest::GTestMain gtest)

add_library(GTest::GMock STATIC IMPORTED)
set_target_properties(
    GTest::GMock
    PROPERTIES INTERFACE_INCLUDE_DIRECTORIES $<BUILD_INTERFACE:${source_dir}/googlemock/include>
               IMPORTED_LOCATION ${binary_dir}/lib/libgmock.a)
add_dependencies(GTest::GMock gtest)

add_library(GTest::GMockMain STATIC IMPORTED)
set_target_properties(
    GTest::GMockMain
    PROPERTIES INTERFACE_INCLUDE_DIRECTORIES $<BUILD_INTERFACE:${source_dir}/googlemock/include>
               IMPORTED_LOCATION ${binary_dir}/lib/libgmock_main.a)
add_dependencies(GTest::GMockMain gtest)

if (Threads_FOUND)
    target_link_libraries(GTest::GTest INTERFACE ${CMAKE_THREAD_LIBS_INIT})
    target_link_libraries(GTest::GMock INTERFACE ${CMAKE_THREAD_LIBS_INIT})
else ()
    target_compile_definitions(GTest::GTest PUBLIC GTEST_HAS_PTHREAD=0)
    target_compile_definitions(GTest::GMock PUBLIC GTEST_HAS_PTHREAD=0)
endif ()
