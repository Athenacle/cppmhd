find_package(PkgConfig QUIET)
include(FindPackageHandleStandardArgs)
pkg_check_modules(PC_CARES QUIET libcares)

find_path(CARES_INCLUDE_DIR NAMES ares.h HINTS ${PC_CARES_INCLUDEDIR} ${PC_CARES_INCLUDE_DIRS})

find_library(CARES_LIBRARY NAMES libcares cares HINTS ${PC_CARES_LIBDIR} ${PC_CARES_LIBRARY_DIRS})

if (PC_CARES_VERSION)
    set(CARES_VERSION_STRING ${PC_MHD_VERSION})
endif ()

if (PC_CARES_FOUND)
    set(CARES_LIBRARIES ${MHD_LIBRARY})
    set(CARES_INCLUDE_DIR ${MHD_INCLUDE_DIR})
    set(CARES_FOUND TRUE)

    if (NOT TARGET CARES::CARES)
        add_library(CARES::CARES STATIC IMPORTED)
        set_target_properties(
            CARES::CARES PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${CARES_INCLUDE_DIR}
                                    IMPORTED_LOCATION ${CARES_LIBRARY})
    endif ()

    mark_as_advanced(CARES_INCLUDE_DIR CARES_LIBRARY CARES_FOUND)
    find_package_handle_standard_args(CARES REQUIRED_VARS CARES_LIBRARY CARES_INCLUDE_DIR
                                      VERSION_VAR CARES_VERSION_STRING)
endif ()
