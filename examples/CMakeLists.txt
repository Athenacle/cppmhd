add_executable(bench bench.cc)

add_sanitizers(bench)

if (TARGET staticlib)
    target_link_libraries(bench PRIVATE staticlib)
else()
    target_link_libraries(bench PRIVATE sharedlib)
endif()