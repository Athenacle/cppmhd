if (NOT MHD_FOUND)

    find_package(PkgConfig QUIET)
    pkg_check_modules(PC_MHD QUIET libmicrohttpd)

    find_path(MHD_INCLUDE_DIR NAMES microhttpd.h HINTS ${PC_MHD_INCLUDEDIR} ${PC_MHD_INCLUDE_DIRS}
                                                       ${mhd_ROOT_DIR})

    find_library(
        MHD_LIBRARY_RELEASE NAMES libmicrohttpd microhttpd libmicrohttpd-dll libmicrohttpd-dll
        HINTS ${PC_MHD_LIBDIR} ${PC_MHD_LIBRARY_DIRS} ${mhd_ROOT_DIR})
    find_library(MHD_LIBRARY_DEBUG NAMES libmicrohttpd-dll_d
                 HINTS ${PC_MHD_LIBDIR} ${PC_MHD_LIBRARY_DIRS} ${mhd_ROOT_DIR})
    if (MHD_LIBRARY_RELEASE)
        set(MHD_LIBRARY ${MHD_LIBRARY_RELEASE})
    else ()
        set(MHD_LIBRARY ${MHD_LIBRARY_DEBUG})
    endif ()

    if (PC_MHD_VERSION)
        set(MHD_VERSION_STRING ${PC_MHD_VERSION})
    elseif (MHD_INCLUDE_DIR AND EXISTS "${MHD_INCLUDE_DIR}/microhttpd.h")
        set(regex_mhd_version "^#define[ \t]+MHD_VERSION[ \t]+([^\"]+).*")
        file(STRINGS "${MHD_INCLUDE_DIR}/microhttpd.h" mhd_version REGEX "${regex_mhd_version}")
        string(REGEX REPLACE "${regex_mhd_version}" "\\1" MHD_VERSION_NUM "${mhd_version}")
        unset(regex_mhd_version)
        unset(mhd_version)
        # parse MHD_VERSION from numerical format 0x12345678 to string format "12.34.56.78" so the version value can be compared to the one returned by pkg-config
        string(SUBSTRING ${MHD_VERSION_NUM} 2 2 MHD_VERSION_STRING_MAJOR)
        string(SUBSTRING ${MHD_VERSION_NUM} 4 2 MHD_VERSION_STRING_MINOR)
        string(SUBSTRING ${MHD_VERSION_NUM} 6 2 MHD_VERSION_STRING_REVISION)
        string(SUBSTRING ${MHD_VERSION_NUM} 8 2 MHD_VERSION_STRING_PATCH)
        set(MHD_VERSION_STRING
            "${MHD_VERSION_STRING_MAJOR}.${MHD_VERSION_STRING_MINOR}.${MHD_VERSION_STRING_REVISION}.${MHD_VERSION_STRING_PATCH}"
            )
    endif ()

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(MHD REQUIRED_VARS MHD_LIBRARY MHD_INCLUDE_DIR
                                      VERSION_VAR MHD_VERSION_STRING)

    if (MHD_FOUND)
        set(MHD_LIBRARIES ${MHD_LIBRARY})
        set(MHD_INCLUDE_DIRS ${MHD_INCLUDE_DIR})

        if (NOT MHD_LIBRARY_DEBUG)
            set(MHD_LIBRARY_DEBUG ${MHD_LIBRARY_RELEASE})
        endif ()

        if (NOT TARGET MHD::MHD)
            add_library(MHD::MHD STATIC IMPORTED)
            set_target_properties(
                MHD::MHD
                PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${MHD_INCLUDE_DIR}
                           IMPORTED_LOCATION_DEBUG ${MHD_LIBRARY_DEBUG}
                           IMPORTED_LOCATION ${MHD_LIBRARY}
                           IMPORTED_LOCATION_RELEASE ${MHD_LIBRARY_RELEASE})

        endif ()
    endif ()

    mark_as_advanced(MHD_INCLUDE_DIR MHD_LIBRARY)

endif ()
