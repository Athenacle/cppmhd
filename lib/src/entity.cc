#include "entity.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>

#include "utils.h"

using namespace cppmhd;

MHDHttpRequest::MHDHttpRequest(MHD_Connection* con, char const* url, HttpMethod mth)
    : HttpRequest(con), uri(url), mhd(mth)
{
    state_ = RequestState::INITIAL;
}

const std::string& MHDHttpRequest::getParam(const std::string& name) const
{
    auto f = param_.find(name);
    if (f == param_.end()) {
        return global::empty;
    } else {
        return f->second;
    }
}

const char* MHDHttpRequest::getHeader(const char* header) const
{
    auto conn = reinterpret_cast<MHD_Connection*>(connection());
    assert(conn);
    return MHD_lookup_connection_value(conn, MHD_HEADER_KIND, header);
}

HttpRequest::~HttpRequest() {}

HttpResponse::~HttpResponse()
{
    clearBody();
}

std::string& HttpResponse::header(const std::string& key)
{
    return headers_[key];
}

std::string& HttpResponse::header(const std::string& key, std::string&& value)
{
    return header(key) = std::move(value);
}

void HttpResponse::clearBody()
{
    if (std::get<2>(body_)) {
        auto ptr = std::get<1>(body_);
        if (ptr) {
            delete[](reinterpret_cast<const uint8_t*>(ptr));
        }
    }
}

void HttpResponse::body(const std::string& data)
{
    body(data.length(), data.c_str(), false);
    header(CPPMHD_HTTP_HEADER_CONTENT_TYPE) = CPPMHD_HTTP_MIME_TEXT_PLAIN;
}

void HttpResponse::body(size_t size, const void* data, bool persistent)
{
    if (unlikely((size == 0 || data == nullptr))) {
        return;
    }

    const void* save = data;
    if (!persistent) {
        save = dumpMemory(data, size);
    }
    clearBody();
    body_ = std::make_tuple(size, save, !persistent);
}