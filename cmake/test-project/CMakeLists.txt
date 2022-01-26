cmake_minimum_required(VERSION 3.14)

set(PN cmake-build-test)
project(${PN})

find_package(cppmhd)

add_executable(${PN} app.cc)
target_link_libraries(${PN} cppmhd::sharedlib)
