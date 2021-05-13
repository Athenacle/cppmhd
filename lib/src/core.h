#ifndef CPPMHD_INTERNAL_CORE_H_
#define CPPMHD_INTERNAL_CORE_H_

#include "config.h"

#include <cppmhd/core.h>

#ifdef ENABLE_FMT_COMPILE
#include <fmt/compile.h>
#endif

#if 0
void* operator new(std::size_t sz) noexcept(false);

void operator delete(void* ptr) noexcept;
#endif

#ifdef ENABLE_FMT_COMPILE
#define FORMAT_CHECK_(f) FMT_COMPILE(f)
#elif defined ENABLE_FMT_STRING
#define FORMAT_CHECK_(f) FMT_STRING(f)
#endif

#ifndef FORMAT_CHECK_
#define FORMAT_CHECK_
#endif

#define FORMAT(f, ...) fmt::format(FORMAT_CHECK_(f), ##__VA_ARGS__)


#if __cplusplus >= 201304L
#define CPPMHD_CONSTEXPR constexpr
#else
#define CPPMHD_CONSTEXPR inline
#endif

#endif