find_package(Threads REQUIRED)
include(ExternalProject)

set(GTEST_ROOT ${CMAKE_CURRENT_BINARY_DIR}/gtest/root CACHE FILEPATH "")

ExternalProject_Add(
    gtest
    URL https://github.com/google/googletest/archive/release-1.11.0.zip
    URL_HASH SHA256=353571c2440176ded91c2de6d6cd88ddd41401d14692ec1f99e35d013feda55a
    DOWNLOAD_NO_PROGRESS ON
    BUILD_BYPRODUCTS
        ${GTEST_ROOT}/src/gtest-build/lib/libgmock.a ${GTEST_ROOT}/src/gtest-build/lib/libgtest.a
        ${GTEST_ROOT}/src/gtest-build/lib/libgtest_main.a
        ${GTEST_ROOT}/src/gtest-build/lib/libgmock_main.a
    PREFIX ${GTEST_ROOT}
    CMAKE_ARGS -Wno-deprecated -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
               -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
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
    PROPERTIES IMPORTED_GLOBAL TRUE INTERFACE_INCLUDE_DIRECTORIES
                                    $<BUILD_INTERFACE:${source_dir}/googletest/include>
               IMPORTED_LOCATION ${binary_dir}/lib/libgtest.a)

add_dependencies(GTest::GTest gtest)
add_library(GTest::gtest ALIAS GTest::GTest)

add_library(GTest::GTestMain STATIC IMPORTED)
set_target_properties(
    GTest::GTestMain
    PROPERTIES INTERFACE_INCLUDE_DIRECTORIES $<BUILD_INTERFACE:${source_dir}/googletest/include>
               IMPORTED_LOCATION ${binary_dir}/lib/libgtest_main.a)
add_dependencies(GTest::GTestMain gtest)

add_library(GTest::GMock STATIC IMPORTED)
set_target_properties(
    GTest::GMock
    PROPERTIES IMPORTED_GLOBAL TRUE INTERFACE_INCLUDE_DIRECTORIES
                                    $<BUILD_INTERFACE:${source_dir}/googlemock/include>
               IMPORTED_LOCATION ${binary_dir}/lib/libgmock.a)
add_dependencies(GTest::GMock gtest)
add_library(GTest::gmock ALIAS GTest::GMock)

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
