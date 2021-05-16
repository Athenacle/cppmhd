include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckCXXSourceCompiles)
include(CheckCXXCompilerFlag)
include(CheckCXXSymbolExists)

check_cxx_symbol_exists(bzero strings.h HAVE_BZERO)
check_cxx_symbol_exists(alloca alloca.h HAVE_ALLOCA)

check_include_file(winsock.h HAVE_INC_WINSOCK)
check_include_file(arpa/inet.h HAVE_INC_ARPA_INET)
check_include_file(netdb.h HAVE_INC_NETDB)

check_cxx_symbol_exists(sigaction signal.h HAVE_SIGACTION)

if (UNIX)
    check_include_file(pthread.h HAVE_PTHREAD_H)
    check_cxx_symbol_exists(pthread_sigmask signal.h HAVE_PTHREAD_SIGMASK)
    check_cxx_symbol_exists(getopt_long unistd.h UNIX_HAVE_GETOPTLONG)
    check_cxx_symbol_exists(dlopen dlfcn.h UNIX_HAVE_DLOPEN)
    check_cxx_symbol_exists(nanosleep time.h UNIX_HAVE_NANOSLEEP)
    check_cxx_symbol_exists(get_nprocs sys/sysinfo.h UNIX_HAVE_GET_NPROCS)
    check_cxx_symbol_exists(setenv cstdlib UNIX_HAVE_SETENV)

    check_cxx_symbol_exists(mmap sys/mman.h _UNIX_HAVE_MMAP)
    check_cxx_symbol_exists(munmap sys/mman.h _UNIX_HAVE_MUNMAP)

    if (${_UNIX_HAVE_MMAP} AND ${_UNIX_HAVE_MUNMAP})
        set(UNIX_HAVE_MMAP ON)
    endif ()
endif ()

if (WIN32)
    check_include_file(WinDns.h WIN_HAVE_INC_WINDNS)
    check_cxx_symbol_exists(DnsQuery_A WinDns.h WIN_HAVE_DNS_QUERY_A)
endif ()

check_cxx_source_compiles(
    "
    #include <sys/types.h>
    #include <unistd.h>
    #include <sys/wait.h>
    int main(){
        int i = fork();
        int status;
        waitpid(i, &status, 0);
    }
    "
    HAVE_UNIX_FORK_WAITPID)

check_cxx_source_compiles(
    "
    #include <time.h>
    int main(){
        return clock_gettime(CLOCK_REALTIME_COARSE, nullptr);
    }
    "
    HAVE_CLOCK_REALTIME_COARSE)

check_cxx_source_compiles(
    "
    int main() {
        return __builtin_expect(0, 1);
    }"
    HAVE_BUILTIN_EXPECT)

if (BUILD_FUZZ AND (CMAKE_C_COMPILER_ID MATCHES "(Apple)?[Cc]lang" OR CMAKE_CXX_COMPILER_ID MATCHES
                                                                      "(Apple)?[Cc]lang"))
    set(COMPILER_SUPPORT_FUZZER ON CACHE BOOL "Support fuzzer" FORCE)
endif ()

macro (CXX_COMPILER_CHECK_ADD)
    set(list_var "${ARGN}")
    foreach (flag IN LISTS list_var)
        string(TOUPPER ${flag} FLAG_NAME1)
        string(REPLACE "-" "_" FLAG_NAME2 ${FLAG_NAME1})
        string(REPLACE "/" "_" FLAG_NAME3 ${FLAG_NAME2})
        string(CONCAT FLAG_NAME "COMPILER_SUPPORT_" ${FLAG_NAME3})
        if (MSVC)
            check_cxx_compiler_flag(/${flag} ${FLAG_NAME})
        else ()
            check_cxx_compiler_flag(-${flag} ${FLAG_NAME})
        endif ()
        if (${${FLAG_NAME}})
            if (MSVC)
                add_compile_options(/${flag})
            else ()
                add_compile_options(-${flag})
            endif ()
        endif ()
    endforeach ()
endmacro ()

if (MSVC)
    cxx_compiler_check_add(
        W4
        ZI #program database for edit and continue
        nologo
        MP #MultiProcess
        Oi #Intrinsic
        wd4820 # padding
        wd4625 # copy ctor implicitly deleted
        wd4626 # operator = implicitly deleted
        wd4668 # macro ... not defined, treat as 0
        wd4577 # noexcept .....
        wd4514 # unreferenced inline function remove
        wd4710 # function ... not inlined
        wd5045 # spectre mitigation
        )
endif ()

if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    cxx_compiler_check_add(
        Wall
        Wno-useless-cast
        Wextra
        Wpedantic
        Wduplicated-branches
        Wduplicated-cond
        Wlogical-op
        Wrestrict
        Wnull-dereference
        Wno-variadic-macros
        Wno-gnu-zero-variadic-macro-arguments
        fno-permissive)

    if (NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Debug")
    endif ()

    if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
        cxx_compiler_check_add(fstack-protector-strong)
    endif ()

    if (CMAKE_CXX_COMPILER_LAUNCHER)
        cxx_compiler_check_add(fdiagnostics-color=always)
    endif ()
endif ()

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ON_64BITS ON)
endif ()

if (WIN32)
    set(ON_WINDOWS ON)
elseif (UNIX)
    set(ON_UNIX ON)
endif ()
