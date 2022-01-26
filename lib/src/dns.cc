#include "config.h"

#ifdef HAVE_INC_NETDB
#include <netdb.h>
#endif

#define FORMAT_INETADDRESS

#include "format.h"
#include "logger.h"
#include "utils.h"

#ifdef ENABLE_CARES
#include <ares.h>

using namespace cppmhd;

bool InetAddress::inited = false;

void cleanUpCares()
{
    std::lock_guard<std::mutex> _(global::mutex);

    ares_library_cleanup();
    InetAddress::inited = true;
}

struct CaresQueryObject {
    uint16_t port_;
    const std::string& query_;
    std::vector<InetAddress>& out_;

    CaresQueryObject(const std::string& q, std::vector<InetAddress>& out) : query_(q), out_(out) {}
};


void DnsQueryCallback(void* arg, int status, int, struct hostent* result)
{
    auto obj = reinterpret_cast<CaresQueryObject*>(arg);
    assert(obj);

    if (unlikely(result == nullptr || status != ARES_SUCCESS)) {
        LOG_ERROR("Failed to lookup '{}': {}", obj->query_, ares_strerror(status));
        return;
    }

    for (auto i = 0u; result->h_addr_list[i]; i++) {
        InetAddress addr;
        auto type = result->h_addrtype;
        auto data = result->h_addr_list[i];
        if (type == AF_INET) {
            sockaddr_in sock;
            memcpy(&sock.sin_addr, data, result->h_length);
            if (InetAddress::from(addr, &sock, obj->port_)) {
                LOG_INFO("Reslove '{}' to '{:h}'", obj->query_, addr);
                obj->out_.emplace_back(addr);
            }
        } else if (type == AF_INET6) {
            sockaddr_in6 sock;
            memcpy(&sock.sin6_addr, data, result->h_length);
            if (InetAddress::from(addr, &sock, obj->port_)) {
                LOG_INFO("Reslove '{}' to '{:h}'", obj->query_, addr);
                obj->out_.emplace_back(addr);
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
            LOG_ERROR("c-ares libraries init failed: {}", ares_strerror(sc));
            return false;
        } else {
            atexit(cleanUpCares);
            inited = true;
        }
    }

    ares_channel ch;
    sc = ares_init(&ch);
    if (sc != ARES_SUCCESS) {
        LOG_ERROR("c-ares init channel failed: {}", ares_strerror(sc));
        return false;
    }

    CaresQueryObject obj(in, out);
    obj.port_ = port;

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

    return obj.out_.size() > 0;
}
#elif defined ON_WINDOWS

#include <WinDNS.h>
#include <WinSock.h>
#pragma comment(lib, "Dnsapi.lib")


bool InetAddress::fromHost(std::vector<InetAddress>& out, const std::string& in, uint16_t port)
{
    WORD type = DNS_TYPE_A;
    DWORD option = DNS_QUERY_STANDARD;
    DNS_RECORD* res;
    auto rc = DnsQuery(in.c_str(), type, option, nullptr, &res, nullptr);

    if (rc == ERROR_SUCCESS) {
        InetAddress addr;
        auto cur = res;
        while (cur) {
            if (cur->wType == DNS_TYPE_A) {
                auto& ip = cur->Data.A.IpAddress;
                struct sockaddr_in sock;
                sock.sin_addr.s_addr = ip;
                sock.sin_family = AF_INET;
                sock.sin_port = htons(port);
                if (InetAddress::from(addr, &sock, port)) {
                    LOG_INFO("Reslove '{}' to '{:h}'", in, addr);
                    out.emplace_back(addr);
                }
            }
            cur = cur->pNext;
        }
    }
    DnsRecordListFree(res, DnsFreeRecordList);
    return out.size() > 0;
}

#else
#error "InetAddress::fromDomain has not been implemented"
#endif
