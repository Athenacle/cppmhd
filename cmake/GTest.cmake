find_package(GTest CONFIG QUIET)

if (NOT GTest_FOUND AND UNIX)
    include(GetGTest)
endif ()
