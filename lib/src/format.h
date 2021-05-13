#ifndef CPPMHD_INTERNAL_FORMAT_H_
#define CPPMHD_INTERNAL_FORMAT_H_

#include "config.h"

#include <fmt/format.h>
using fmt::format_parse_context;
using fmt::formatter;

#include <cassert>

#include "utils.h"

using namespace cppmhd;

#ifdef FORMAT_REQUEST_STATE
#include "entity.h"
#endif

namespace fmt
{
#if !defined NDEBUG && defined FORMAT_REQUEST_STATE

template <>
struct formatter<RequestState> {
    FMT_CONSTEXPR auto parse(format_parse_context& ctx) -> decltype(ctx.begin())
    {
        auto it = ctx.begin();
        return it;
    }

    template <typename FormatContext>
    auto format(const RequestState& mtd, FormatContext& ctx) -> decltype(ctx.out())
    {
        const char* ptr = "UNKNOWN";

#define BUILD_REQUEST_STATE(MTD) \
    case RequestState::MTD: {    \
        ptr = #MTD;              \
        break;                   \
    }
        switch (mtd) {
            BUILD_REQUEST_STATE(INITIAL)
            BUILD_REQUEST_STATE(INITIAL_COMPLETE)
            BUILD_REQUEST_STATE(DATA_RECEIVING)
            BUILD_REQUEST_STATE(DATA_RECEIVED)
            BUILD_REQUEST_STATE(ERROR)
        }
        return format_to(ctx.out(), "{}", ptr);
#undef BUILD_REQUEST_STATE
    }
};


#endif

#ifdef FORMAT_HTTP_METHOD
template <>
struct formatter<HttpMethod> {
    FMT_CONSTEXPR auto parse(format_parse_context& ctx) -> decltype(ctx.begin())
    {
        auto it = ctx.begin();
        return it;
    }

    template <typename FormatContext>
    auto format(const HttpMethod& mtd, FormatContext& ctx) -> decltype(ctx.out())
    {
        const char* ptr = "UNKNOWN";

#define BUILD_HTTP_METHOD_STRING(MTD) \
    case HttpMethod::MTD: {           \
        ptr = #MTD;                   \
        break;                        \
    }

        switch (mtd) {
            BUILD_HTTP_METHOD_STRING(GET)
            BUILD_HTTP_METHOD_STRING(HEAD)
            BUILD_HTTP_METHOD_STRING(POST)
            BUILD_HTTP_METHOD_STRING(PUT)
            BUILD_HTTP_METHOD_STRING(DELETE)
            BUILD_HTTP_METHOD_STRING(OPTIONS)
            BUILD_HTTP_METHOD_STRING(CONNECT)
            BUILD_HTTP_METHOD_STRING(TRACE)
            BUILD_HTTP_METHOD_STRING(PATCH)
        }
        return format_to(ctx.out(), "{}", ptr);

#undef BUILD_HTTP_METHOD_STRING
    }
};

#endif

#ifdef FORMAT_INETADDRESS

#ifndef FORMAT_SOCKADDR_IN
#define FORMAT_SOCKADDR_IN
#endif

#ifndef FORMAT_SOCKADDR_IN6
#define FORMAT_SOCKADDR_IN6
#endif

#endif

#ifdef FORMAT_SOCKADDR_IN
template <>
struct formatter<sockaddr_in> {
    char pres;

    FMT_CONSTEXPR auto parse(format_parse_context& ctx) -> decltype(ctx.begin())
    {
        auto it = ctx.begin(), end = ctx.end();
        pres = 'a';

        if (it != end && (*it == 'a' || *it == 'h' || *it == 'p'))
            pres = *it++;

        return it;
    }

    template <typename FormatContext>
    auto format(const sockaddr_in& p, FormatContext& ctx) -> decltype(ctx.out())
    {
        if (pres == 'p') {
            return format_to(ctx.out(), "{}", ntohs(p.sin_port));
        } else {
            const size_t size = INET6_ADDRSTRLEN + 1;
            char buffer[size];

            MAYBE_UNUSED auto ret = inet_ntop(AF_INET, &p.sin_addr, buffer, INET_ADDRSTRLEN);
            assert(ret == buffer);
            if (pres == 'h') {
                return format_to(ctx.out(), "{}", buffer);
            }

            return format_to(ctx.out(), "{}:{}", buffer, ntohs(p.sin_port));
        }
    }
};
#endif

#ifdef FORMAT_SOCKADDR_IN6
template <>
struct formatter<sockaddr_in6> {
    char pres;

    FMT_CONSTEXPR auto parse(format_parse_context& ctx) -> decltype(ctx.begin())
    {
        auto it = ctx.begin(), end = ctx.end();
        pres = 'a';

        if (it != end && (*it == 'a' || *it == 'h' || *it == 'p'))
            pres = *it++;

        return it;
    }

    template <typename FormatContext>
    auto format(const sockaddr_in6& p, FormatContext& ctx) -> decltype(ctx.out())
    {
        if (pres == 'p') {
            return format_to(ctx.out(), "{}", ntohs(p.sin6_port));
        } else {
            const size_t size = INET6_ADDRSTRLEN + 1;
            char buffer[size];

            MAYBE_UNUSED auto ret = inet_ntop(AF_INET6, &p.sin6_addr, buffer, INET6_ADDRSTRLEN);
            assert(ret == buffer);
            if (pres == 'h') {
                return format_to(ctx.out(), "{}", buffer);
            }

            return format_to(ctx.out(), "{}:{}", buffer, ntohs(p.sin6_port));
        }
    }
};
#endif

#ifdef FORMAT_INETADDRESS
template <>
struct formatter<InetAddress> {
    char pres;

    FMT_CONSTEXPR auto parse(format_parse_context& ctx) -> decltype(ctx.begin())
    {
        pres = 'a';
        auto it = ctx.begin(), end = ctx.end();

        if (it != end && (*it == 'a' || *it == 'h' || *it == 'p'))
            pres = *it++;

        return it;
    }

    template <typename FormatContext>
    auto format(const InetAddress& p, FormatContext& ctx) -> decltype(ctx.out())
    {
        auto sock = p.getSocket();

        char f[5];
        f[0] = '{', f[1] = ':', f[2] = pres, f[3] = '}', f[4] = 0;

        if (likely(!p.isV6())) {
            auto sock4 = reinterpret_cast<const sockaddr_in*>(sock);
            auto& addr = *sock4;
            return format_to(ctx.out(), f, addr);
        } else {
            auto sock6 = reinterpret_cast<const sockaddr_in6*>(sock);
            auto& addr = *sock6;
            return format_to(ctx.out(), f, addr);
        }
    }
};
#endif

}  // namespace fmt

#endif