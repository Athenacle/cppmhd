find_package(PkgConfig QUIET)
include(FindPackageHandleStandardArgs)

set(_PCRE2_VERSIONS 8 16 32 posix)

set(_PCRE2_LIBS)
set(_PCRE2_INDIR)
set(_PCRE2_VERSION)

foreach (_pc IN LISTS _PCRE2_VERSIONS)
    pkg_check_modules(_PCRE2_${_pc} libpcre2-${_pc})
    find_path(_PCRE2_${_pc}_INCLUDE_DIR NAMES pcre2.h
              HINTS ${_PCRE2_${_pc}_INCLUDEDIR} ${_PCRE2_${_pc}_INCLUDE_DIRS} ${pcre2_ROOT_DIR})
    find_library(_PCRE2_${_pc}_LIBRARY_RELEASE NAMES pcre2-${_pc}
                 HINTS ${_PCRE2_${_pc}_LIBDIR} ${_PCRE2_${_pc}_LIBRARY_DIRS} ${pcre2_ROOT_DIR})
    find_library(_PCRE2_${_pc}_LIBRARY_DEBUG NAMES pcre2-${_pc}d
                 HINTS ${_PCRE2_${_pc}_LIBDIR} ${_PCRE2_${_pc}_LIBRARY_DIRS} ${pcre2_ROOT_DIR})
    set(_PCRE2_${_comp}_LIBRARY)

    if (_PCRE2_${_pc}_LIBRARY_RELEASE OR _PCRE2_${_pc}_LIBRARY_DEBUG)
        add_library(PCRE2::${_pc} MODULE IMPORTED)
        set_target_properties(PCRE2::${_pc} PROPERTIES INCLUDE_DIRECTORIES
                                                       ${_PCRE2_${_pc}_INCLUDE_DIR})
    endif ()

    if (_PCRE2_${_pc}_LIBRARY_DEBUG)
        set_property(TARGET PCRE2::${_pc} APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(PCRE2::${_pc} PROPERTIES IMPORTED_LOCATION_DEBUG
                                                       ${_PCRE2_${_pc}_LIBRARY_DEBUG})
        list(APPEND _PCRE2_LIBS "debug" ${_PCRE2_${_pc}_LIBRARY_DEBUG})
        list(APPEND _PCRE2_${_comp}_LIBRARY "debug" ${_PCRE2_${_pc}_LIBRARY_DEBUG})
    endif ()

    if (_PCRE2_${_pc}_LIBRARY_RELEASE)
        set_property(TARGET PCRE2::${_pc} APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(PCRE2::${_pc} PROPERTIES IMPORTED_LOCATION_RELEASE
                                                       ${_PCRE2_${_pc}_LIBRARY_RELEASE})
        list(APPEND _PCRE2_LIBS "optimized" ${_PCRE2_${_pc}_LIBRARY_RELEASE})
        list(APPEND _PCRE2_${_comp}_LIBRARY "optimized" ${_PCRE2_${_pc}_LIBRARY_RELEASE})
    endif ()

    set(_PCRE2_INDIR ${_PCRE2_${_pc}_INCLUDE_DIR})
    set(_PCRE2_VERSION ${_PCRE2_${_pc}_VERSION})
    mark_as_advanced(_PCRE2_${_comp}_LIBRARY)
endforeach (_pc IN LISTS _PCRE2_VERSIONS)

set(PCRE2_INCLUDE_DIR ${_PCRE2_INDIR})
set(PCRE2_LIBRARIES ${_PCRE2_LIBS})

if ((NOT ${_PCRE2_VERSION}) OR ("${_PCRE2_VERSION}" STREQUAL ""))
    file(READ "${_PCRE2_INDIR}/pcre2.h" _PCRE2_INC OFFSET 0 LIMIT 8192)
    string(LENGTH "${_PCRE2_INC}" _PCRE2_INC_LEN)
    string(REGEX MATCH "#define[ \t]+PCRE2_MAJOR[ \t]+[0-9]+" _PCRE2_MAJOR_S "${_PCRE2_INC}")
    string(REGEX MATCH "#define[ \t]+PCRE2_MINOR[ \t]+[0-9]+" _PCRE2_MINOR_S "${_PCRE2_INC}")
    string(REGEX MATCH "[ \t]+[0-9]+" _PCRE2_MAJOR "${_PCRE2_MAJOR_S}")
    string(REGEX MATCH "[ \t]+[0-9]+" _PCRE2_MINOR "${_PCRE2_MINOR_S}")
    string(STRIP "${_PCRE2_MAJOR}" MAJ_V)
    string(STRIP "${_PCRE2_MINOR}" MIN_V)
    set(PCRE2_VERSION "${MAJ_V}.${MIN_V}")
else ()
    set(PCRE2_VERSION ${_PCRE2_VERSION})
endif ()

foreach (_comp IN LISTS PCRE2_FIND_COMPONENTS)
    if (TARGET PCRE2::${_comp})

        get_target_property(_pc_${_comp}_lib_debug PCRE2::${_comp} IMPORTED_LOCATION_DEBUG)
        get_target_property(_pc_${_comp}_lib_release PCRE2::${_comp} IMPORTED_LOCATION_RELEASE)
        set(_pc_${_comp}_lib)
        if (_pc_${_comp}_lib_debug)
            list(APPEND _pc_${_comp}_lib "debug" ${_pc_${_comp}_lib_debug})
            list(APPEND PCRE2_LIBRARIES "debug" ${_pc_${_comp}_lib_debug})
        endif ()

        if (_pc_${_comp}_lib_release)
            list(APPEND _pc_${_comp}_lib "optimized" ${_pc_${_comp}_lib_release})
            list(APPEND PCRE2_LIBRARIES "optimized" ${_pc_${_comp}_lib_release})
        endif ()

        message(STATUS "PCRE-${_comp} found: ${_pc_${_comp}_lib} version ${PCRE2_VERSION}")
        set(PCRE2_${_comp}_FOUND TRUE)
    else ()
        set(PCRE2_${_comp}_FOUND FALSE)
    endif ()
endforeach (_comp IN LISTS PCRE2_FIND_COMPONENTS)

find_package_handle_standard_args(PCRE2 REQUIRED_VARS PCRE2_LIBRARIES PCRE2_INCLUDE_DIR
                                  VERSION_VAR PCRE2_VERSION)
