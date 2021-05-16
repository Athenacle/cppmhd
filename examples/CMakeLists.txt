add_executable(bench bench.cc)

add_sanitizers(bench)

target_link_libraries(bench PRIVATE cppmhd::lib)
