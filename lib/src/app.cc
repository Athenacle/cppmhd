#include "config.h"

#include <cppmhd/app.h>

#include <microhttpd.h>
#include <signal.h>

#include <cassert>

#include "http.h"
#include "logger.h"
#include "router.h"
#include "utils.h"

using namespace cppmhd;

int cppmhd_version()
{
    return PROJECT_VERSION_HEX;
}

const char* cppmhd_version_string()
{
    return PROJECT_VERSION_STRING;
}

namespace
{
void* (*mallocFn)(size_t) = std::malloc;
void (*freeFn)(void*) = std::free;

static const auto defaultEH =
    [](const HttpRequestPtr&, HttpStatusCode sc, HttpError error, const std::string& msg) -> HttpResponsePtr {
    return defaultErrorHandler(sc, error, msg);
};

}  // namespace

#if 0
void* operator new(std::size_t sz) noexcept(false)
{
    void* ptr = mallocFn((sz));

    if (likely(ptr != nullptr))
        return ptr;

    throw std::bad_alloc();
}

void operator delete(void* ptr) noexcept
{
    freeFn(ptr);
}
#endif

CPPMHD_NAMESPACE_BEGIN

void* cpp_mhd_malloc(unsigned long size)
{
    return mallocFn(size);
}

void cpp_mhd_free(void* ptr)
{
    freeFn(ptr);
}


void cpp_mhd_set_malloc(void*(mallocF)(size_t), void (*freeF)(void*))
{
    mallocFn = mallocF;
    freeFn = freeF;
}


/// App funcs

struct AppManager {
    static AppManager manager;

    using HandlerItem = std::tuple<int, std::function<void(App&, int)>>;

    std::mutex handlersMutex;
    std::vector<std::tuple<int, std::function<void(App&, int)>>> handlers;

    std::vector<App*> apps;

    AppManager() = default;

    void listSignals(std::vector<int>& out)
    {
        std::lock_guard<std::mutex> _(handlersMutex);
        for (auto& h : handlers) {
            out.emplace_back(std::get<0>(h));
        }
    }

    bool have(int sig)
    {
        std::lock_guard<std::mutex> _(handlersMutex);
        auto begin = AppManager::manager.handlers.begin();
        auto end = AppManager::manager.handlers.end();
        auto f = std::find_if(begin, end, [sig](const AppManager::HandlerItem& p) { return std::get<0>(p) == sig; });
        return f != end;
    }
};

AppManager AppManager::manager;

void signalHandlerWrapper(int signal)
{
    std::lock_guard<std::mutex> _(AppManager::manager.handlersMutex);
    std::lock_guard<std::mutex> __(global::mutex);

    auto begin = AppManager::manager.handlers.begin();
    auto end = AppManager::manager.handlers.end();
    auto f = std::find_if(begin, end, [signal](const AppManager::HandlerItem& p) { return std::get<0>(p) == signal; });

    assert(f != end);
    // LOG_INFO("signal {} received.", strsignal(signal));
    if (f != end) {
        for (auto& app : AppManager::manager.apps) {
            std::get<1> (*f)(*app, signal);
        }
    }
}

bool App::setSignalHandler(int signal, const signalHandler& sh)
{
    std::lock_guard<std::mutex> _(AppManager::manager.handlersMutex);

    auto begin = AppManager::manager.handlers.begin();
    auto end = AppManager::manager.handlers.end();
    auto f = std::find_if(begin, end, [signal](const AppManager::HandlerItem& p) { return std::get<0>(p) == signal; });

    if (registerSignalHandler(signal, signalHandlerWrapper)) {
        if (f == end) {
            AppManager::manager.handlers.emplace_back(signal, sh);
        } else {
            std::get<1>(*f) = sh;
        }
        return true;
    } else {
        return false;
    }
}

bool App::isRunning() const
{
    return http_ && http_->isRunning();
}

App ::~App()
{
    assert(!isRunning());

    std::lock_guard<std::mutex> _(AppManager::manager.handlersMutex);
    auto f = std::find(AppManager::manager.apps.begin(), AppManager::manager.apps.end(), this);
    assert(f != AppManager::manager.apps.end());
    AppManager::manager.apps.erase(f);

    if (http_) {
        delete http_;
    }
}

App::App(const std::string& addr, uint16_t port) : address_(addr), port_(port)
{
    http_ = nullptr;

    threadCount_ = getNProc();
    eh = defaultEH;
    {
        std::lock_guard<std::mutex> _(AppManager::manager.handlersMutex);
        AppManager::manager.apps.emplace_back(this);
    }
}

int App::start(const std::function<void(void)>& cb)
{
    InetAddress addr;
    if (InetAddress::parse(addr, address_, port_)) {
        Router r(std::move(builder_));

        if (r.build()) {
            http_ = new HttpImplement(addr, std::move(r), host_, eh);

            if (!AppManager::manager.have(SIGINT)) {
                setSignalHandler(SIGINT, [](App& app, int) {
                    // LOG_INFO("SIGINT received. {}", "Stopping...");
                    app.stop();
                });
            }
#ifdef SIGPIPE
            if (!AppManager::manager.have(SIGPIPE)) {
                setSignalHandler(SIGPIPE, [](App&, int) {});
            }
#endif

            std::vector<int> sigs;
            AppManager::manager.listSignals(sigs);
            return http_->startMHDDaemon(threadCount_, cb, sigs);
        } else {
            return CPPMHD_ROUTER_TREE_BUILD_FAILED;
        }
    } else {
        LOG_ERROR("Parse listen address {}:{} failed...", address_, port_);
        return CPPMHD_LISTEN_ADDRESS_ERROR;
    }
}

int App::start()
{
    return start([]() {});
}

void App::stop()
{
    if (isRunning())
        http_->stop();
}


CPPMHD_NAMESPACE_END