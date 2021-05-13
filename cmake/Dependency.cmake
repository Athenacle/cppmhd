include(FindPkgConfig)

pkg_check_modules(libmicrohttpd REQUIRED libmicrohttpd)
pkg_check_modules(libpcre2-8 REQUIRED libpcre2-8)
pkg_check_modules(libcares libcares)
find_package(fmt REQUIRED)

set(ENABLE_PCRE2_8 ${libpcre2-8_FOUND})
set(ENABLE_CARES ${libcares_FOUND})