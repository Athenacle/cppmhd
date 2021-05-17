#include "config.h"

#include <cppmhd/app.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread>

#include "curl.h"
#include "logger.h"
#include "utils.h"

#ifdef TEST_ENABLE_CURL
#include <curl/curl.h>
#else
#error "This should not been compiled without TEST_ENABLE_CURL defined"
#endif

#ifdef TEST_ENABLE_OPENSSL
#include <openssl/sha.h>
#endif

using namespace testing;
using namespace cppmhd;

struct ShaCalc {
#ifdef TEST_ENABLE_OPENSSL
    SHA256_CTX ctx;
    unsigned char hash[SHA256_DIGEST_LENGTH];
#else
    uint64_t start;
#endif
    bool finished;
    mutable std::mutex mutex;

    ShaCalc& operator=(const ShaCalc&);

    ShaCalc();

    void update(const void* in, size_t size);

    void final();

    void reset();

    operator bool() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return finished;
    }

    bool operator==(const ShaCalc& other) const;

    std::string format() const;
};

bool calcFile(ShaCalc& calc, const std::string fullName);

class TestCtrl : public HttpController
{
  public:
    MOCK_METHOD((void), onConnection, (HttpRequestPtr, HttpResponsePtr&), (override));

    MOCK_METHOD((void), onRequest, (HttpRequestPtr, HttpResponsePtr&), (override));

    virtual ~TestCtrl() {}
};

class SimpleConnectionMock : public ActionInterface<void(HttpRequestPtr, HttpResponsePtr&)>
{
  public:
    SimpleConnectionMock() = default;
    ~SimpleConnectionMock() = default;

    void Perform(const std::tuple<HttpRequestPtr, HttpResponsePtr&>&) {}
};

#define MOCK_CTRL(func) func(_, _)

#define DEFAULT_MOCK_CONNECTION(mock) \
    ON_CALL(*mock, MOCK_CTRL(onConnection)).WillByDefault(MakeAction(new SimpleConnectionMock))

#define DEFAULT_MOCK_CONNECTION_TIMES(mock, times) EXPECT_CALL(*mock, MOCK_CTRL(onConnection)).Times(times)

class SimpleRequestMock : public ActionInterface<void(HttpRequestPtr, HttpResponsePtr&)>
{
    HttpStatusCode code;
    std::string body;

  public:
    SimpleRequestMock() : SimpleRequestMock(k200OK) {}

    SimpleRequestMock(HttpStatusCode code) : SimpleRequestMock(code, "") {}

    SimpleRequestMock(HttpStatusCode code, const std::string b) : code(code), body(b) {}

    ~SimpleRequestMock() = default;


    void Perform(const std::tuple<HttpRequestPtr, HttpResponsePtr&>& args)
    {
        auto& resp = std::get<1>(args);
        resp = std::make_shared<HttpResponse>();
        if (body.length() > 0) {
            resp->body("hello!");
            resp->header(CPPMHD_HTTP_HEADER_CONTENT_TYPE) = CPPMHD_HTTP_MIME_TEXT_PLAIN;
        }
        resp->status(code);
    }
};

#define DEFAULT_MOCK_REQUEST_WITH_PARAMTER(mock, param) \
    ON_CALL(*mock, MOCK_CTRL(onRequest)).WillByDefault(MakeAction(new SimpleRequestMock param))

#define DEFAULT_MOCK_REQUEST(mock) DEFAULT_MOCK_REQUEST_WITH_PARAMTER(mock, ())

#define DEFAULT_MOCK_REQUEST_TIMES(mock, times) EXPECT_CALL(*mock, MOCK_CTRL(onRequest)).Times(times)

class HttpApp : public Test
{
    static uint8_t lv;

    std::mutex mutex_;

    void run();

  protected:
    App* app;
    Barrier startUp;

    std::thread thr;

    std::string myName;

    void start();

    template <class... Args>
    HttpController* add(Args&&... args)
    {
        return app->builder().add(std::forward<Args>(args)...);
    }

    template <class T, class... Args>
    T* add(Args&&... args)
    {
        return app->builder().add<T>(std::forward<Args>(args)...);
    }

    Curl curl(const std::string& url)
    {
        return Curl(host, port, url);
    }

    Curl curl()
    {
        return curl(myName);
    }

  public:
    static const uint16_t port;
    // = 65432;
    static const std::string host;
    // = "127.0.0.1";

    HttpApp() : startUp(2) {}
    void SetUp();

    void TearDown();

    static void SetUpTestSuite()
    {
        lv = log::getLevel();
        log::setLogLevel(log::kError);
    }

    static void TearDownTestSuite()
    {
        log::setLogLevel(static_cast<log::LogLevel>(lv));
    }
};