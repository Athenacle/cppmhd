#include "config.h"

#include <netdb.h>

#define FORMAT_INETADDRESS

#include "format.h"
#include "logger.h"
#include "utils.h"

#ifdef ENABLE_CARES
#include <ares.h>
#else
#error "InetAddress::fromDomain has not been implemented"
#endif

using namespace cppmhd;

#ifdef ENABLE_CARES

bool InetAddress::inited = false;

void cleanUpCares()
{
    std::lock_guard<std::mutex> _(global::mutex);

    ares_library_cleanup();
    InetAddress::inited = true;
}

struct CaresQueryObject {
    int status;
    uint16_t port;
    const std::string& query;
    std::vector<InetAddress>& out;

    CaresQueryObject(const std::string& q, std::vector<InetAddress>& out) : query(q), out(out) {}
};


void DnsQueryCallback(void* arg, int status, int, struct hostent* result)
{
    auto obj = reinterpret_cast<CaresQueryObject*>(arg);
    assert(obj);

    if (unlikely(result == nullptr || status != ARES_SUCCESS)) {
        ERROR("Failed to lookup '{}': {}", obj->query, ares_strerror(status));
        return;
    }

    for (auto i = 0u; result->h_addr_list[i]; i++) {
        InetAddress addr;
        auto type = result->h_addrtype;
        auto data = result->h_addr_list[i];
        if (type == AF_INET) {
            auto sock = reinterpret_cast<sockaddr_in*>(data);

            if (InetAddress::from(addr, sock, obj->port)) {
                INFO("Reslove '{}' to '{:h}'", obj->query, addr);
                obj->out.emplace_back(addr);
            }
        } else if (type == AF_INET6) {
            auto sock = reinterpret_cast<sockaddr_in6*>(data);
            if (InetAddress::from(addr, sock, obj->port)) {
                INFO("Reslove '{}' to '{:h}'", obj->query, addr);
                obj->out.emplace_back(addr);
            }
        }
    }
}

bool InetAddress::fromHost(std::vector<InetAddress>& out, const std::string& in, uint16_t port)
{
    int sc;
    if (unlikely(!inited)) {
        std::lock_guard<std::mutex> _(global::mutex);

        sc = ares_library_init(ARES_LIB_INIT_ALL);

        if (unlikely(sc != 0)) {
            ERROR("c-ares libraries init failed: {}", ares_strerror(sc));
            return false;
        } else {
            atexit(cleanUpCares);
            inited = true;
        }
    }

    ares_channel ch;
    sc = ares_init(&ch);
    if (sc != ARES_SUCCESS) {
        ERROR("c-ares init channel failed: {}", ares_strerror(sc));
        return false;
    }

    CaresQueryObject obj(in, out);
    obj.port = port;

    ares_gethostbyname(ch, in.c_str(), AF_INET, DnsQueryCallback, &obj);

    {
        MAYBE_UNUSED int nfds, count;
        fd_set readers, writers;
        timeval tv, *tvp;
        while (1) {
            FD_ZERO(&readers);
            FD_ZERO(&writers);
            nfds = ares_fds(ch, &readers, &writers);
            if (nfds == 0)
                break;
            tvp = ares_timeout(ch, nullptr, &tv);
            count = select(nfds, &readers, &writers, nullptr, tvp);
            ares_process(ch, &readers, &writers);
        }
    }

    ares_destroy(ch);

    return obj.out.size() > 0;
}
#endif
