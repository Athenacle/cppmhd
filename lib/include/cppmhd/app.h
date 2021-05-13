#ifndef CPPMHD_SERVER_H_
#define CPPMHD_SERVER_H_

#include <cppmhd/core.h>
#include <cppmhd/entity.h>
#include <cppmhd/router.h>

#include <functional>
#include <string>

CPPMHD_NAMESPACE_BEGIN

enum CPPMHD_Error {
    // everything works well
    CPPMHD_OK = 0,

    // eg: App("aaa.aaa.aa.a",80) -> invalid IP address
    CPPMHD_LISTEN_ADDRESS_ERROR = 1,

    CPPMHD_LISTEN_FAILED = 2,

    CPPMHD_ROUTER_TREE_BUILD_FAILED

};

enum class HttpError {
    OK,

    // method specified
    DATA_IN_GET_REQUEST,
    DATA_PROCESSOR_NOT_SET,
    DATA_PROCESSOR_RETURN_ERROR,

    // method not allowed
    BAD_HTTP_METHOD,
    BAD_HTTP_VERSION,

    // router error
    ROUTER_NOT_FOUND,
    TSR_FOUND,
    HOST_FIELD_INCORRECT
};

class App
{
  public:
    using errorHandler =
        std::function<HttpResponsePtr(const HttpRequestPtr &, HttpStatusCode, HttpError, const std::string &)>;
    using signalHandler = std::function<void(App &, int)>;

  private:
    std::string address_;
    uint16_t port_;
    RouterBuilder builder_;

    HttpImplement *http_;

    uint32_t threadCount_;

    errorHandler eh;

    std::string host_;

  public:
    App(const std::string &addr, uint16_t port);
    ~App();

    int start();

    int start(const std::function<void(void)> &);

    void stop();

    uint32_t threadCount()
    {
        return threadCount_;
    }

    void threadCount(uint32_t tc)
    {
        if (!isRunning()) {
            threadCount_ = tc;
        }
    }

    template <class... Args>
    inline HttpController *add(Args &&...args)
    {
        return builder_.add(std::forward<Args>(args)...);
    }

    template <class T, class... Args>
    inline T *add(Args &&...args)
    {
        return builder_.add<T>(std::forward<Args>(args)...);
    }

    RouterBuilder &builder()
    {
        return builder_;
    }

    void setErrorHandler(errorHandler &&handler);

    bool isRunning() const;

    static bool setSignalHandler(int, const signalHandler &sh);

    const std::string &host() const
    {
        return host_;
    }

    std::string &host()
    {
        return host_;
    }
};

CPPMHD_NAMESPACE_END

#endif