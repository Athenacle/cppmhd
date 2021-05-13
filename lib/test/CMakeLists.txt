aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} UNITTEST_SRC)

if (TEST_ENABLE_CURL)
    aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/http HTTP_SRC)
endif ()

include(GTest)
include(GoogleTest)

enable_testing()

add_executable(unittest ${UNITTEST_SRC} ${HTTP_SRC})

target_link_libraries(
    unittest
    PRIVATE cppmhd::lib GTest::GTest GTest::GMock "$<$<BOOL: TEST_ENABLE_OPENSSL>:OpenSSL::Crypto>"
            "$<$<BOOL: TEST_ENABLE_CURL>:CURL::libcurl>")

target_include_directories(unittest PRIVATE ${CMAKE_SOURCE_DIR}/lib/src ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(unittest PRIVATE TESTING)
target_compile_definitions(unittest PRIVATE "-DUNITTEST_FILE=$<TARGET_FILE:unittest>"
                                            "-DUNITTEST_FILENAME=$<TARGET_FILE_BASE_NAME:unittest>")

gtest_discover_tests(unittest)

add_sanitizers(unittest)

if (ENABLE_FUZZER)
    add_subdirectory(fuzz)
endif ()
