#ifndef MHD_INTERNAL_HTTPIMPLEMENT_H_
#define MHD_INTERNAL_HTTPIMPLEMENT_H_

#include "config.h"

#include <cppmhd/app.h>
#include <cppmhd/core.h>

#include <microhttpd.h>

#include <atomic>
#include <thread>

#include "core.h"
#include "router.h"
#include "utils.h"

#if MHD_VERSION >= 0x00097200
using MHD_Return = MHD_Result;
#define MHD_OK MHD_Result::MHD_YES
#define MHD_FAILED MHD_Result::MHD_NO;
#else
using MHD_Return = int;
#define MHD_OK ((1))
#define MHD_FAILED ((0))
#endif

CPPMHD_NAMESPACE_BEGIN

class HttpImplement
{
    std::vector<MHD_Daemon *> daemons;

    InetAddress addr_;
    Barrier runningBarrier_;
    Router router_;
    std::thread thr_;
    std::atomic_bool running_;
    const App::errorHandler &eh_;

    std::string host_;

    bool logConnectionStatus_;

  public:
    HttpImplement(const InetAddress &ad, Router &&r, std::string &host, const App::errorHandler &eh)
        : addr_(ad), runningBarrier_(2), router_(std::move(r)), eh_(eh), host_(host)
    {
        running_ = false;

        logConnectionStatus_ =
#ifdef NDEBUG
            false;
#else
            true;
#endif
    }

    bool isLogConnectionStatus()
    {
        return logConnectionStatus_;
    }

    const App::errorHandler &getErrorHandler() const
    {
        return eh_;
    }

    CPPMHD_Error startMHDDaemon(uint32_t threadCount,
                                const std::function<void(void)> &cb,
                                const std::vector<int> &sigs);

    HttpController *forward(HttpRequest *req, std::map<std::string, std::string> &param, bool &tsr) const
    {
        return router_.forward(req, param, tsr);
    }

    void stop();

    bool isV6() const
    {
        return addr_.isV6();
    }

    const InetAddress &getAddr() const
    {
        return addr_;
    }

    bool isRunning() const
    {
        return running_;
    }

    HttpResponsePtr checkRequest(const HttpRequestPtr &, const char *);

    void addSharedHeaders(HttpResponsePtr &);
};

CPPMHD_NAMESPACE_END

#endif