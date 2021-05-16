#ifndef CPPMHD_ENTITY_H_
#define CPPMHD_ENTITY_H_

#include <cppmhd/core.h>

#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <tuple>

CPPMHD_NAMESPACE_BEGIN

class HttpRequest
{
    std::shared_ptr<DataProcessor> dp_;

    // for internal HttpRequest implement use.
    mutable MHD_Connection* conn_;

  protected:
    HttpRequest(MHD_Connection* conn) : conn_(conn)
    {
        dp_ = nullptr;
    }

  public:
    HttpRequest()
    {
        dp_ = nullptr;
        conn_ = nullptr;
    }

    MHD_Connection* connection() const
    {
        return conn_;
    }

    std::shared_ptr<DataProcessor> processor()
    {
        return dp_;
    }
    void setProcessor(std::shared_ptr<DataProcessor> dp)
    {
        dp_ = dp;
    }

    virtual const char* getPath() const = 0;

    std::string getPathString() const
    {
        return std::string(getPath());
    }

    virtual ~HttpRequest();

    virtual HttpMethod getMethod() const = 0;

    virtual const char* getHeader(const char* header) const = 0;

    virtual const std::string& getParam(const std::string& name) const = 0;
};

using HttpRequestPtr = std::shared_ptr<HttpRequest>;

class HttpResponse
{
    struct HttpHeaderCompare {
        bool operator()(const std::string& first, const std::string& second) const
        {
#if _WIN32
            return _stricmp(first.c_str(), second.c_str()) < 0;
#else
            return strncasecmp(first.c_str(), second.c_str(), std::min(first.length(), second.length())) < 0;
#endif
        }
    };

  public:
    using HeaderType = std::map<std::string, std::string, HttpHeaderCompare>;

    using BodyType = std::tuple<size_t, const void*, bool>;

  private:
    HttpStatusCode sc;

    HeaderType headers_;

    BodyType body_;

    void clearBody();

  public:
    HttpResponse()
    {
        body_ = std::make_tuple(0, nullptr, false);
    }

    ~HttpResponse();

    void body(size_t size, const void* data, bool persistent);

    void body(size_t size, const void* data)
    {
        body(size, data, false);
    }

    const BodyType& body() const
    {
        return body_;
    }

    void body(const std::string& data);

    const HeaderType& headers() const
    {
        return headers_;
    }

    std::string& header(const std::string& key);

    std::string& header(const std::string& key, std::string&& value);

    void status(HttpStatusCode sc)
    {
        this->sc = sc;
    }

    HttpStatusCode status() const
    {
        return sc;
    }
};
using HttpResponsePtr = std::shared_ptr<HttpResponse>;

CPPMHD_NAMESPACE_END

#endif