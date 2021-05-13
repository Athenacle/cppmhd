#ifndef CPPMHD_INTERNAL_UTILS_H_
#define CPPMHD_INTERNAL_UTILS_H_

#include "config.h"

#include <cppmhd/app.h>
#include <cppmhd/core.h>

#include <arpa/inet.h>
#include <pthread.h>

#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "core.h"
#include "entity.h"

CPPMHD_NAMESPACE_BEGIN

namespace global
{
extern const std::string empty;
extern std::mutex mutex;
}  // namespace global

HttpResponsePtr defaultErrorHandler(HttpStatusCode sc, HttpError error, const std::string &msg);

inline void *dumpMemory(const void *data, size_t size)
{
    auto ret = new uint8_t[size];
    memcpy(ret, data, size);
    return ret;
}

void split(const std::string &in, std::vector<std::string> &out, const std::string &sep);

const char *dispatchErrorCode(HttpStatusCode sc);

uint32_t getNProc();

typedef void (*signalHandler)(int);
bool registerSignalHandler(int signal, signalHandler sh);

inline bool checkPrefix(const char *first, const char *second, size_t len)
{
    return strncmp(first, second, len) == 0;
}

template <size_t N>
inline bool isPrefixOfArray(const char *ct, const char (&cp)[N])
{
    if (ct == nullptr) {
        return false;
    }
    return checkPrefix(ct, cp, std::min(strlen(ct), N - 1));
}

class Regex
{
  public:
    using Group = std::map<std::string, std::string>;

  private:
    void *re_;
    const char *pattern_;


  public:
    void swap(Regex &other)
    {
        std::swap(re_, other.re_);
        std::swap(pattern_, other.pattern_);
    }

    bool operator==(const Regex &other) const
    {
        return pattern_ != nullptr && other.pattern_ != nullptr && strcmp(pattern_, other.pattern_) == 0;
    }

    operator bool() const
    {
        return re_ != nullptr;
    }

    Regex(Regex &&that) : Regex()
    {
        this->swap(that);
    }

    Regex()
    {
        pattern_ = nullptr;
        re_ = nullptr;
    }

    ~Regex();

    Regex &operator=(Regex &&that)
    {
        std::swap(re_, that.re_);
        std::swap(pattern_, that.pattern_);
        return *this;
    }

    static bool compileRegex(Regex &, const char *);

    static bool compileRegex(Regex &re, const std::string &pattern)
    {
        return compileRegex(re, pattern.c_str());
    }

    bool match(const char *) const;

    bool match(const std::string &) const;

    bool match(const char *begin, const char *end) const;

    bool match(const std::string &, Group &) const;

    bool match(const char *, size_t) const;

    bool match(const char *, size_t, Group &) const;
};

class Barrier
{
    pthread_barrier_t *bar;

  public:
    Barrier()
    {
        bar = nullptr;
    }

    Barrier(Barrier &&that)
    {
        std::swap(bar, that.bar);
    }

    explicit Barrier(uint32_t n)
    {
        bar = new pthread_barrier_t;
        pthread_barrier_init(bar, nullptr, n);
    }
    ~Barrier()
    {
        if (likely(bar)) {
            pthread_barrier_destroy(bar);
            delete bar;
        }
    }

    int wait()
    {
        return pthread_barrier_wait(bar);
    }
};

class InetAddress
{
    bool ipv6{false};
    uint16_t port_;

    union Addr {
        struct sockaddr_in v4;
        struct sockaddr_in6 v6;
    } address;

  public:
#ifdef ENABLE_CARES
    static bool inited;
#endif

    uint16_t port() const
    {
        return port_;
    }
    bool isV6() const
    {
        return ipv6;
    }

    CPPMHD_CONSTEXPR size_t socketLen() const
    {
        if (ipv6) {
            return sizeof(address.v6);
        } else {
            return sizeof(address.v4);
        }
    }

    const sockaddr *getSocket() const
    {
        if (ipv6) {
            return reinterpret_cast<const sockaddr *>(&address.v6);
        } else {
            return reinterpret_cast<const sockaddr *>(&address.v4);
        }
    }

    InetAddress()
    {
        memset(this, 0, sizeof(*this));
    }

    InetAddress(const InetAddress &that)
    {
        memcpy(&this->address.v6, &that.address.v6, sizeof(this->address.v6));
        ipv6 = that.ipv6;
        port_ = that.port_;
    }

    static bool parse(InetAddress &out, const std::string &in, uint16_t port);

    static bool fromHost(std::vector<InetAddress> &out, const std::string &in, uint16_t port);

    static bool from(InetAddress &out, sockaddr_in *in, uint16_t port)
    {
        memcpy(&out.address.v4.sin_addr, in, sizeof(out.address.v4.sin_addr));
        out.ipv6 = false;
        out.address.v4.sin_family = AF_INET;
        out.address.v4.sin_port = htons((port));
        out.port_ = port;
        return true;
    }

    static bool from(InetAddress &out, sockaddr_in6 *in, uint16_t port)
    {
        memcpy(&out.address.v6.sin6_addr, in, sizeof(out.address.v6.sin6_addr));
        out.ipv6 = true;
        out.address.v6.sin6_family = AF_INET6;
        out.address.v6.sin6_port = htons((port));
        out.port_ = port;
        return true;
    }
};

CPPMHD_NAMESPACE_END

#endif