
#include "config.h"

#ifdef TEST_ENABLE_OPENSSL
#include <openssl/sha.h>
#endif

#include "http_app.h"
#include "test.h"

namespace
{
std::vector<size_t> inSeq;
#define CHECK_IN_SEQ                                                    \
    do {                                                                \
        ASSERT_TRUE(inSeq.size() >= 1 && inSeq[inSeq.size() - 1] == 0); \
        inSeq.clear();                                                  \
    } while (false)
}  // namespace

class HttpSha256Processor : public HttpDataProcessor<ShaCalc>
{
  public:
    HttpSha256Processor() = default;
    virtual ~HttpSha256Processor() {}

    virtual size_t onData(HttpRequestPtr&, ShaCalc& sha, const void* in, size_t size) override
    {
        inSeq.emplace_back(size);
        if (size != 0) {
            sha.update(in, size);
        } else {
            sha.final();
        }
        return size;
    }
};

template <class Sha256Processor>
class HttpSha256Controller : public DataProcessorController<ShaCalc, Sha256Processor>
{
    mutable ShaCalc calc_;

  public:
    HttpSha256Controller() = default;
    virtual ~HttpSha256Controller() {}

    const ShaCalc& calc() const
    {
        return calc_;
    }

    virtual void onRequest(HttpRequestPtr, const ShaCalc& data, HttpResponsePtr& resp) override
    {
        calc_ = data;
        resp = std::make_shared<HttpResponse>();
        resp->status(k200OK);
        resp->body(data.format());
    }
};

class HttpStringDataProcessor : public HttpDataProcessor<std::string>
{
  public:
    HttpStringDataProcessor()
    {
        LOG_DTRACE("{}::{} this = {}", "HttpStringDataProcessor", __func__, (void*)this);
    }
    virtual ~HttpStringDataProcessor() {}

    virtual size_t onData(HttpRequestPtr&, std::string& d, const void* data, size_t size) override
    {
        inSeq.emplace_back(size);
        LOG_DTRACE(
            "{}::{} this = {} => data ptr {}, size {}", "HttpStringDataProcessor", __func__, (void*)this, data, size);
        d.append(reinterpret_cast<const char*>(data), size);
        return size;
    }
};

class StringDataCollectorCtrl : public DataProcessorController<std::string, HttpStringDataProcessor>
{
    std::string result_;

  public:
    virtual void onRequest(HttpRequestPtr, const std::string& data, HttpResponsePtr& resp) override
    {
        LOG_DTRACE("{}: data: {}, size {}", __func__, data, data.size());
        result_ = data;
        resp = std::make_shared<HttpResponse>();
        resp->status(k200OK);
        resp->body(result_);
    }

    virtual ~StringDataCollectorCtrl() {}
    StringDataCollectorCtrl()
    {
        LOG_DTRACE("{}::{} this = {}", "StringDataCollectorCtrl", __func__, (void*)this);
    }

    void result(std::string& r)
    {
        r = result_;
    }
};

TEST_F(HttpApp, stringProcessor)
{
    auto mock = add<StringDataCollectorCtrl>(HttpMethod::PUT, myName);
    start();

    Curl c = curl();
    c.method(HttpMethod::PUT);
    c.upload("hello?");
    c.perform();

    std::string result;
    mock->result(result);

    EXPECT_EQ(c.status(), k200OK);
    EXPECT_EQ(result, "hello?");
    CHECK_IN_SEQ;
}

TEST_F(HttpApp, noDataProcessor)
{
    auto mock = add<TestCtrl>(HttpMethod::PUT, myName);
    start();

    DEFAULT_MOCK_REQUEST(mock);
    DEFAULT_MOCK_CONNECTION(mock);
    DEFAULT_MOCK_REQUEST_TIMES(mock, 0);
    DEFAULT_MOCK_CONNECTION_TIMES(mock, 1);

    Curl b(curl());
    Curl c;
    c = std::move(b);
    c.method(HttpMethod::PUT);
    c.upload("hello?");
    c.perform();
    EXPECT_EQ(c.status(), k405MethodNotAllowed);
}

TEST_F(HttpApp, sha256)
{
    inSeq.clear();
    auto mock = add<HttpSha256Controller<HttpSha256Processor>>(HttpMethod::PUT, myName);
    start();

    auto size = rand() % 8192 + 8192;

    auto block = generateRandom(size);

    ShaCalc calc;
    calc.update(block, size);
    calc.final();

    Curl c(host, port, myName);
    c.upload(block, size, CPPMHD_HTTP_MIME_APPLICATION_OCTET);
    c.method(HttpMethod::PUT);
    c.perform();
    EXPECT_EQ(c.status(), k200OK);

    EXPECT_EQ(mock->calc(), calc);
    EXPECT_EQ(c.body(), calc.format());

    CHECK_IN_SEQ;

    delete[] block;
}

class HttpSha256GreaterProcessor : public HttpDataProcessor<ShaCalc>
{
    // note: this test processor return value always grater than size
  public:
    HttpSha256GreaterProcessor() = default;
    virtual ~HttpSha256GreaterProcessor() {}

    virtual size_t onData(HttpRequestPtr&, ShaCalc& sha, const void* in, size_t size) override
    {
        inSeq.emplace_back(size);
        if (size != 0) {
            sha.update(in, size);
        } else {
            sha.final();
        }
        return size + 1;
    }
};
TEST_F(HttpApp, sha256Larger)
{
    inSeq.clear();
    // this test body is same as TEST_F(HttpApp, sha256)
    // as DataProcessor return grater than size just cause a warning, not an error
    auto mock = add<HttpSha256Controller<HttpSha256GreaterProcessor>>(HttpMethod::PUT, myName);
    start();

    auto size = rand() % 8192 + 8192;

    auto block = generateRandom(size);

    ShaCalc calc;
    calc.update(block, size);
    calc.final();

    Curl c(host, port, myName);
    c.upload(block, size, CPPMHD_HTTP_MIME_APPLICATION_OCTET);
    c.method(HttpMethod::PUT);
    c.perform();
    EXPECT_EQ(c.status(), k200OK);

    CHECK_IN_SEQ;
    EXPECT_EQ(mock->calc(), calc);
    EXPECT_EQ(c.body(), calc.format());

    delete[] block;
}


class HttpSha256ErrorProcessor : public HttpDataProcessor<ShaCalc>
{
    // note: this test processor return value always grater than size

  public:
    HttpSha256ErrorProcessor() = default;
    virtual ~HttpSha256ErrorProcessor() {}

    virtual size_t onData(HttpRequestPtr&, ShaCalc& sha, const void* in, size_t size) override
    {
        inSeq.emplace_back(size);
        if (size != 0) {
            sha.update(in, size);
        } else {
            sha.final();
        }
        return DataProcessor::DataProcessorParseFailed;
    }
};
TEST_F(HttpApp, sha256Error)
{
    inSeq.clear();
    add<HttpSha256Controller<HttpSha256ErrorProcessor>>(HttpMethod::PUT, myName);
    start();

    auto size = (rand() % 8192 + 8192) << 1;

    auto block = generateRandom(size);

    Curl c(host, port, myName);
    c.upload(block, size, CPPMHD_HTTP_MIME_APPLICATION_OCTET);
    c.method(HttpMethod::PUT);
    c.perform();
    EXPECT_EQ(c.status(), k400BadRequest);

    delete[] block;
}