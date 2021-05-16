#include "curl.h"

#include "config.h"

#include <algorithm>

#include "core.h"
#include "logger.h"
#include "utils.h"

#define FORMAT_HTTP_METHOD
#include "format.h"

using namespace cppmhd;

#ifdef TESTING
#include <gtest/gtest.h>
#endif

#ifdef min
#define MIN min
#else
#define MIN std::min
#endif

#define CURL_PROCESS(expr)                                                              \
    do {                                                                                \
        auto ret = (expr);                                                              \
        if (ret != CURLE_OK) {                                                          \
            LOG_ERROR("Curl error {} ({}) -> {}", curl_easy_strerror(ret), #expr, url); \
        }                                                                               \
        EXPECT_EQ(ret, CURLE_OK);                                                       \
    } while (false)

namespace
{
size_t curlWriteDataCB(void *data, size_t size, size_t nmemb, void *userp)
{
    auto realsize = size * nmemb;
    auto curl = reinterpret_cast<Curl *>(userp);
    curl->in(data, realsize);
    return realsize;
}

size_t curlHeaderCB(char *b, size_t size, size_t nitems, void *userp)
{
    static Regex headerRe;

    auto curl = reinterpret_cast<Curl *>(userp);

    if (!headerRe) {
        Regex::compileRegex(headerRe, "(?<header>[\\w\\-]+)\\s*:\\s*(?<value>.+)\\r");
    }

    size_t numbytes = size * nitems;
    std::map<std::string, std::string> group;

    if (headerRe.match(b, numbytes, group)) {
        curl->header(std::move(group["header"]), std::move(group["value"]));
    }
    return numbytes;
}
size_t uploadData(char *ptr, size_t size, size_t nmemb, void *userp)
{
    auto curl = reinterpret_cast<Curl *>(userp);
    auto total = size * nmemb;
    auto data = curl->upload(total);
    if (total != 0) {
        memcpy(ptr, data, total);
    }

    return total;
}
}  // namespace


void Curl::setTimeout(long to)
{
    CURL_PROCESS(curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2000l));
}

void Curl::perform()
{
    if (reqHeader) {
        CURL_PROCESS(curl_easy_setopt(curl, CURLOPT_HTTPHEADER, reqHeader));
    }
    if (form) {
        CURL_PROCESS(curl_easy_setopt(curl, CURLOPT_MIMEPOST, form));
    }
    CURL_PROCESS(curl_easy_perform(curl));
    CURL_PROCESS(curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_));
}

void Curl::addRequestHeader(const std::string &key, const std::string &value)
{
    auto header = FORMAT("{}:{}", key, value);
    reqHeader = curl_slist_append(reqHeader, header.c_str());
}

void Curl::followLocation()
{
    CURL_PROCESS(curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L));
}

void Curl::method(HttpMethod mtd)
{
    method_ = FORMAT("{}", mtd);
    CURLoption next = CURLOPT_LASTENTRY;
    switch (mtd) {
        case HttpMethod::GET:
            next = CURLOPT_HTTPGET;
            break;
        case HttpMethod::POST:
            next = CURLOPT_POST;
            break;
        case HttpMethod::PUT:
            next = CURLOPT_PUT;
            break;
        default:
            break;
    }
    if (next != CURLOPT_LASTENTRY) {
        CURL_PROCESS(curl_easy_setopt(curl, next, 1));
    } else {
        CURL_PROCESS(curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method_.c_str()));
    }
}

void Curl::setup(CURLoption opt, const void *ptr)
{
    CURL_PROCESS(curl_easy_setopt(curl, opt, ptr));
}

void Curl::setup(CURLoption opt, long value)
{
    CURL_PROCESS(curl_easy_setopt(curl, opt, value));
}

void Curl::setup()
{
    CURL_PROCESS(curl_easy_setopt(curl, CURLOPT_URL, url.c_str()));
    CURL_PROCESS(curl_easy_setopt(curl, CURLOPT_WRITEDATA, this));
    CURL_PROCESS(curl_easy_setopt(curl, CURLOPT_HEADERDATA, this));
    CURL_PROCESS(curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteDataCB));
    CURL_PROCESS(curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curlHeaderCB));
    CURL_PROCESS(curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, ""));
    setTimeout(3);
}

const void *Curl::upload(size_t &size)
{
    auto ptr = reinterpret_cast<const uint8_t *>(std::get<0>(upload_));
    auto totalSize = std::get<1>(upload_);

    if (uploadPos_ >= totalSize) {
        size = 0;
        return nullptr;
    } else {
        auto nextSize = MIN(size, totalSize - uploadPos_);
        auto ret = ptr + uploadPos_;
        size = nextSize;
        uploadPos_ += nextSize;
        return ret;
    }
}

void Curl::mime_upload(const std::string &name, const uint8_t *data, size_t size)
{
    if (!form) {
        form = curl_mime_init(curl);
    }
    auto field = curl_mime_addpart(form);
    CURL_PROCESS(curl_mime_name(field, name.c_str()));
    CURL_PROCESS(curl_mime_data(field, reinterpret_cast<const char *>(data), size));
}

void Curl::mime_upload(const std::string &key, const std::string &value)
{
    if (!form) {
        form = curl_mime_init(curl);
    }
    auto field = curl_mime_addpart(form);
    CURL_PROCESS(curl_mime_name(field, key.c_str()));
    CURL_PROCESS(curl_mime_type(field, CPPMHD_HTTP_MIME_TEXT_PLAIN));
    CURL_PROCESS(curl_mime_data(field, value.c_str(), CURL_ZERO_TERMINATED));
}


void Curl::mime_uploadfile(const std::string &name, const std::string &filename)
{
    if (!form) {
        form = curl_mime_init(curl);
    }

    auto field = curl_mime_addpart(form);
    CURL_PROCESS(curl_mime_name(field, "file"));
    CURL_PROCESS(curl_mime_filename(field, name.c_str()));
    CURL_PROCESS(curl_mime_filedata(field, filename.c_str()));
}

void Curl::mime_upload(const std::map<std::string, std::string> &mime)
{
    if (!form) {
        form = curl_mime_init(curl);
    }

    for (const auto &m : mime) {
        mime_upload(m.first, m.second);
    }
}

void Curl::upload(const void *data, size_t size, const std::string &ct)
{
    auto buf = dumpMemory(data, size);
    upload_ = std::make_tuple(buf, size);

    CURL_PROCESS(curl_easy_setopt(curl, CURLOPT_READFUNCTION, uploadData));
    CURL_PROCESS(curl_easy_setopt(curl, CURLOPT_READDATA, this));

    if (ct.length() > 0) {
        addRequestHeader(CPPMHD_HTTP_HEADER_CONTENT_TYPE, ct);
    }
}

Curl::~Curl()
{
    if (reqHeader) {
        curl_slist_free_all(reqHeader);
    }
    if (form) {
        curl_mime_free(form);
    }

    curl_easy_cleanup(curl);

    if (std::get<0>(upload_) != nullptr) {
        delete[](reinterpret_cast<const uint8_t *>(std::get<0>(upload_)));
    }
}

Curl::Curl(CurlSchema schema, const std::string &host, uint16_t port, const std::string &uri) : Curl()
{
    auto sh = schema == CurlSchema::HTTP ? "http" : "https";
    url = FORMAT("{}://{}:{}{}", sh, host, port, uri);
    setup();
}

Curl::Curl()
{
    curl = curl_easy_init();
    reqHeader = nullptr;
    status_ = BAD_STATUS;
    uploadPos_ = 0;
    upload_ = std::make_tuple(nullptr, 0);
    form = nullptr;
}
