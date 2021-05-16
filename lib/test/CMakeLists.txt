aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} UNITTEST_SRC)

if (TEST_ENABLE_CURL)
    aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/http HTTP_SRC)
endif ()

# find_package(GTest CONFIG REQUIRED)
include(GTest)

enable_testing()

add_executable(unittest ${UNITTEST_SRC} ${HTTP_SRC})

target_link_libraries(
    unittest
    PRIVATE cppmhd::lib GTest::gtest GTest::gmock
            "$<$<TARGET_EXISTS:OpenSSL::Crypto>:OpenSSL::Crypto>"
            "$<$<TARGET_EXISTS:CURL::libcurl>:CURL::libcurl>")

target_include_directories(unittest PRIVATE ${CMAKE_SOURCE_DIR}/lib/src ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(unittest PRIVATE TESTING)
target_compile_definitions(
    unittest
    PRIVATE "-DUNITTEST_FILE=$<TARGET_FILE:unittest>"
            "-DUNITTEST_FILENAME=$<TARGET_FILE_NAME:unittest>"
            "$<$<BOOL:${RUN_ID}>:RUN_ID=${RUN_ID}>")

add_custom_target(run-unittest COMMAND "$<TARGET_FILE:unittest>")

# if (NOT WIN32)
#     include(GoogleTest)
#     gtest_discover_tests(unittest)
# endif ()

add_sanitizers(unittest)

if (ENABLE_FUZZER)
    add_subdirectory(fuzz)
endif ()
