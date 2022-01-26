#ifndef CPP_MHD_INTERNAL_TEST_CURL_
#define CPP_MHD_INTERNAL_TEST_CURL_

#include <cppmhd/core.h>

#include <curl/curl.h>

#include <chrono>
#include <cstring>
#include <map>
#include <string>

using cppmhd::HttpMethod;
struct HttpHeaderCompare {
    bool operator()(const std::string& first, const std::string& second) const
    {
#ifdef WIN32
        return _stricmp(first.c_str(), second.c_str()) < 0;
#else
        return strncasecmp(first.c_str(), second.c_str(), std::min(first.length(), second.length())) < 0;
#endif
    }
};
using HeaderType = std::map<std::string, std::string, HttpHeaderCompare>;

class Curl
{
    std::string response;
    std::string url;
    std::string method_;

    curl_slist* reqHeader;
    curl_mime* form;
    HeaderType headers_;

    CURL* curl;

    void setup();

    int status_;

    std::tuple<const void*, size_t> upload_;
    size_t uploadPos_;

    Curl(const Curl&) = delete;
    Curl& operator=(const Curl&) = delete;

    void setTimeoutMS(uint32_t);

  public:
    Curl(Curl&& other);
    Curl& operator=(Curl&& other)
    {
        std::swap(response, other.response);
        std::swap(url, other.url);
        std::swap(method_, other.method_);
        std::swap(headers_, other.headers_);
        std::swap(reqHeader, other.reqHeader);
        std::swap(curl, other.curl);
        std::swap(status_, other.status_);
        std::swap(upload_, other.upload_);
        std::swap(uploadPos_, other.uploadPos_);
        std::swap(form, other.form);
        return *this;
    }

    void setup(CURLoption opt, long value);
    void setup(CURLoption opt, const void* ptr);

    const int BAD_STATUS = -1;

    enum class CurlSchema { HTTP, HTTPS };

    ~Curl();

    Curl();

    Curl(CurlSchema schema, const std::string& host, uint16_t port, const std::string& uri);

    Curl(const std::string& host, uint16_t port, const std::string& uri) : Curl(CurlSchema::HTTP, host, port, uri) {}

    Curl(const std::string& host, uint16_t port) : Curl(CurlSchema::HTTP, host, port, "/") {}

    Curl(const std::string& host) : Curl(CurlSchema::HTTP, host, 80, "/") {}

    void addRequestHeader(const std::string& key, const std::string& value);

    void method(HttpMethod);

    void in(void* data, size_t size)
    {
        response.append(reinterpret_cast<char*>(data), size);
    }
    HeaderType& headers()
    {
        return headers_;
    }

    void setTimeout(uint32_t seconds)
    {
        return setTimeout(std::chrono::seconds(seconds));
    }

    template <class R, class P>
    void setTimeout(const std::chrono::duration<R, P>& dur)
    {
        setTimeoutMS(std::chrono::duration_cast<std::chrono::milliseconds>(dur).count());
    }

    const std::string& body() const
    {
        return response;
    }
    void header(std::string&& key, std::string&& value)
    {
        headers_.emplace(std::move(key), std::move(value));
    }

    const void* upload(size_t& size);

    void upload(const void* data, size_t size, const std::string& ct);

    void upload(const void* data, size_t size)
    {
        upload(data, size, CPPMHD_HTTP_MIME_APPLICATION_OCTET);
    }

    void upload(const std::string& data, const std::string& ct)
    {
        upload(data.c_str(), data.length(), ct);
    }

    void upload(const std::string& data)
    {
        upload(data.c_str(), data.length(), CPPMHD_HTTP_MIME_TEXT_PLAIN);
    }

    void mime_upload(const std::map<std::string, std::string>& data);
    void mime_upload(const std::string& key, const std::string& value);
    void mime_upload(const std::string& key, const uint8_t* data, size_t size);
    void mime_uploadfile(const std::string& name, const std::string& filename);

    void followLocation();
    void perform();
    int status() const
    {
        return status_;
    }
};

#endif