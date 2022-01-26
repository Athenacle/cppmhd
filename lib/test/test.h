#include <cppmhd/app.h>
#include <cppmhd/entity.h>

#ifndef NO_GMOCK
#include <gmock/gmock.h>
#endif

#include "entity.h"
#include "utils.h"

using namespace cppmhd;

#define STR(x) #x
#define STRINGIFY(x) STR(x)

#ifdef UNITTEST_FILE
#define MY_BINARY_FILE STRINGIFY(UNITTEST_FILE)
#endif

#ifdef UNITTEST_FILENAME
#define MY_BINARY_FILENAME STRINGIFY(UNITTEST_FILENAME)
#endif


class SignalHandler
{
  public:
    SignalHandler() = default;
    virtual void handler(App&, int) = 0;
    virtual ~SignalHandler() {}
};

class SHMock : public SignalHandler
{
  public:
    MOCK_METHOD((void), handler, (App&, int), (override));
    SHMock() = default;
    virtual ~SHMock() {}
};

class TestRequest : public HttpRequest
{
    HttpMethod mtd_;
    std::string path_;
    std::map<std::string, std::string> param_;

  public:
    virtual const char* getPath() const override
    {
        return path_.c_str();
    }

    virtual HttpMethod getMethod() const override
    {
        return mtd_;
    }

    virtual const char* getHeader(const char*) const override
    {
        return nullptr;
    }

    TestRequest(HttpMethod mtd, const std::string p) : mtd_(mtd), path_(p) {}

    virtual ~TestRequest() {}


    virtual const std::string& getParam(const std::string&) const override
    {
        return global::empty;
    }
};

uint8_t* generateRandom(size_t size);