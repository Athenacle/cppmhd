if (NOT TARGET staticlib)
    message(FATAL_ERROR "static lib not found, this should be an error")
endif ()

file(GLOB FUZZ_SRC *.cc)

string(LENGTH ${CMAKE_CURRENT_BINARY_DIR} CURREND_DIR_NAME_LENGTH)

set(FUZZ_DIR_ROOT ${CMAKE_BINARY_DIR}/fuzz/)

foreach (FZ IN LISTS FUZZ_SRC)
    get_filename_component(FZNAME ${FZ} NAME_WE)
    string(CONCAT TNAME ${FZNAME} "-fuzzer")
    add_executable(${TNAME} ${FZ})
    target_link_options(${TNAME} PRIVATE -fsanitize=fuzzer)
    target_compile_definitions(${TNAME} PRIVATE -DNO_GMOCK)
    target_link_libraries(${TNAME} staticlib)
    target_include_directories(${TNAME} PRIVATE ${CMAKE_SOURCE_DIR}/lib/src)
    add_sanitizers(${TNAME})

    set(MY_FUZZ_ROOT "${FUZZ_DIR_ROOT}${TNAME}")
    file(MAKE_DIRECTORY ${MY_FUZZ_ROOT}/corpus)
    file(MAKE_DIRECTORY ${MY_FUZZ_ROOT}/crash)
    file(READ ${FZ} FZ_CONTENT)

    string(REGEX MATCHALL "args: [^\n]+" FZ_ARGS ${FZ_CONTENT})

    set(FUZZ_ARGS)
    foreach (arg IN LISTS FZ_ARGS)
        string(REGEX REPLACE " *args *:? *" "" ARG_OUT ${arg})
        string(REGEX REPLACE "FZ_ROOT" "${CMAKE_CURRENT_SOURCE_DIR}" ROOT_REPLACE ${ARG_OUT})
        list(APPEND FUZZ_ARGS ${ROOT_REPLACE})
    endforeach ()

    add_custom_target(fuzz-${TNAME} COMMAND ${TNAME} "-artifact_prefix=${MY_FUZZ_ROOT}/crash/"
                                            ${FUZZ_ARGS} ${MY_FUZZ_ROOT}/corpus DEPENDS ${TNAME})
endforeach ()
