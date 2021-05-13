#include "utils.h"

#include <cppmhd/app.h>
#include <cppmhd/entity.h>

#include <signal.h>

#include <cassert>
#include <mutex>
#include <thread>

#include "logger.h"

#ifdef ENABLE_PCRE2_8
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#else
#error "Regex without pcre2-8 has not been implemented"
#endif

#define FORMAT_INETADDRESS
#include "format.h"

CPPMHD_NAMESPACE_BEGIN

namespace global
{
std::mutex mutex;
const std::string empty;
}  // namespace global

HttpResponsePtr defaultErrorHandler(HttpStatusCode sc, HttpError, const std::string& msg)
{
    auto err = FORMAT("{} {}", sc, dispatchErrorCode(sc));

    static constexpr char fmt[] =
        "<html>\n"
        "<head><title>{}</title></head>\n"
        "<body>\n"
        "<center><h1>{}</h1>\n"
        "<h2>{}</h2>\n"
        "</center>\n"
        "<hr><center>" PROJECT_SERVER_HEADER
        "</center>\n"
        "</body>\n"
        "</html>\n"
        "<!-- a padding to disable MSIE and Chrome friendly error page -->\n"
        "<!-- a padding to disable MSIE and Chrome friendly error page -->\n"
        "<!-- a padding to disable MSIE and Chrome friendly error page -->\n"
        "<!-- a padding to disable MSIE and Chrome friendly error page -->\n"
        "<!-- a padding to disable MSIE and Chrome friendly error page -->\n"
        "<!-- a padding to disable MSIE and Chrome friendly error page -->\n";

    auto estr = FORMAT(fmt, err, err, msg);

    auto resp = std::make_shared<HttpResponse>();
    resp->status(sc);
    resp->body(estr);
    resp->header(CPPMHD_HTTP_HEADER_CONTENT_TYPE) = CPPMHD_HTTP_MIME_TEXT_HTML;

    return resp;
}

bool registerSignalHandler(int signal, signalHandler sh)
{
    struct sigaction oldsig;
    struct sigaction sig;

    auto name = fmt::to_string(signal);

    sig.sa_handler = sh;
    sigemptyset(&sig.sa_mask);

#ifdef SA_INTERRUPT
    sig.sa_flags = SA_INTERRUPT;
#else
    sig.sa_flags = SA_RESTART;
#endif

    if (0 != sigaction(signal, &sig, &oldsig)) {
        ERROR("failed to install '{}' handler: {}", name, strerror(errno));
        return false;
    } else {
        DEBUG("install '{}' signal success.", name);
        return true;
    }
}

const char* dispatchErrorCode(HttpStatusCode sc)
{
    switch (sc) {
        LIKELY case k100Continue : return "Continue";

        LIKELY case k200OK : return "OK";

        LIKELY case k301MovePermanently : return "Moved Permanently";
        LIKELY case k302Found : return "Found";
        LIKELY case k304NotModified : return "Not Modified";

        LIKELY case k400BadRequest : return "Bad Request";
        LIKELY case k401Unauthorized : return "Unauthorized";
        LIKELY case k403Forbidden : return "Forbidden";
        LIKELY case k404NotFound : return "Not Found";
        LIKELY case k405MethodNotAllowed : return "Method Not Allowed";
        LIKELY case k406NoAcceptable : return "Not Acceptable";

        LIKELY case k500InternalServerError : return "Internal Server Error";
        LIKELY case k501NotImplemented : return "Not Implemented";
        LIKELY case k502BadGateway : return "Bad Gateway";
        LIKELY case k503ServiceUnavailable : return "Service Unavailable";

        case k101SwitchingProtocol:
            return "Switching Protocol";
        case k102Processing:
            return "Processing";
        case k103EarlyHints:
            return "Early Hints";

        case k201Created:
            return "Created";
        case k202Accepted:
            return "Accepted";
        case k203NonAuthoritativeInfomation:
            return "Non-Authoritative Information";
        case k204NoContent:
            return "No Content";
        case k205ResetContent:
            return "Reset Content";
        case k206PartialContent:
            return "Partial Content";

        case k300MultipleChoice:
            return "Multiple Choice";
        case k303SeeOther:
            return "See Other";
        case k305UseProxy:
            return "Use Proxy";
        case k307TemporaryRedirect:
            return "Temporary Redirect";
        case k308PermanentRedirect:
            return "Permanent Redirect";

        case k402PaymentRequired:
            return "Payment Required";
        case k407ProxyAuthenticationRequired:
            return "Proxy Authentication Required";
        case k408RequestTimeout:
            return "Request Timeout";
        case k409Conflict:
            return "Conflict";
        case k410Gone:
            return "Gone";
        case k411LengthRequired:
            return "Length Required";
        case k412PreconditionFailed:
            return "Precondition Failed";
        case k413PayloadTooLarge:
            return "Payload Too Large";
        case k414URITooLong:
            return "URI Too Long";
        case k415UnsupportedMediaType:
            return "Unsupported Media Type";
        case k416RangeNotSatisfiable:
            return "Range Not Satisfiable";
        case k417ExpectationFailed:
            return "Expectation Failed";
        case k418ImaTeapot:
            return "I'm a teapot";
        case k421MisDirectedRequest:
            return "Misdirected Request";
        case k429TooManyRequests:
            return "Too Many Requests";

        case k504GatewayTimeout:
            return "Gateway Timeout";
        case k505HttpVersionNotSupported:
            return "HTTP Version Not Supported";
        case k506VariantAlsoNegotiates:
            return "Variant Also Negotiates";
        default:
            return "Unknown Status Code";
    }
}

bool InetAddress::parse(InetAddress& addr, const std::string& in, uint16_t port)
{
    auto ap = htons(port);
    addr.port_ = port;
    auto status = inet_pton(AF_INET, in.c_str(), &addr.address.v4.sin_addr);
    if (likely(status == 1)) {
        addr.ipv6 = false;
        addr.address.v4.sin_port = ap;
        addr.address.v4.sin_family = AF_INET;
        return true;
    } else {
        status = inet_pton(AF_INET6, in.c_str(), &addr.address.v6.sin6_addr);
        if (status == 1) {
            addr.ipv6 = true;
            addr.address.v6.sin6_port = ap;
            addr.address.v6.sin6_family = AF_INET6;
            return true;
        }
    }
    return false;
}

uint32_t getNProc()
{
    return std::thread::hardware_concurrency();
}

void split(const std::string& in, std::vector<std::string>& out, const std::string& sep)
{
    std::string tmp(in);
    do {
        auto pos = tmp.find_first_of(sep);
        if (pos == std::string::npos) {
            if (tmp.length() > 0) {
                out.emplace_back(tmp);
            }
            break;
        } else if (pos != 0) {
            std::string sub(tmp, 0, pos);
            tmp = tmp.substr(pos + sep.length());
            out.emplace_back(sub);
        } else {
            tmp = tmp.substr(sep.length());
        }

    } while (true);
}

/// regex

#ifdef ENABLE_PCRE2_8

bool Regex::compileRegex(Regex& re, const char* pattern)
{
    int error;
    auto p = reinterpret_cast<PCRE2_SPTR>(pattern);
    PCRE2_SIZE offset;

    if (unlikely(re.re_)) {
        re.~Regex();
    }

    auto ptr = pcre2_compile(p, PCRE2_ZERO_TERMINATED, 0, &error, &offset, nullptr);

    if (unlikely(ptr == nullptr)) {
        PCRE2_UCHAR buffer[256];
        pcre2_get_error_message(error, buffer, sizeof(buffer));
        ERROR("compile regex pattern '{}' failed at offset {}: '{}'", pattern, offset, buffer);
    } else {
        auto pl = std::char_traits<char>::length(pattern);
        auto pt = new char[pl + 1];
        std::char_traits<char>::copy(pt, pattern, pl);
        pt[pl] = 0;
        re.pattern_ = pt;

#ifndef NDEBUG
        TRACE("compile regex pattern '{}': {}", pattern, ptr != nullptr ? "success" : "failed");
#endif
    }

    re.re_ = ptr;
    return ptr != nullptr;
}

bool Regex::match(const char* in, size_t len, Group& g) const
{
    auto data = reinterpret_cast<PCRE2_SPTR>(in);
    auto ptr = reinterpret_cast<pcre2_code*>(re_);
    int rc = 0;
    auto md = pcre2_match_data_create_from_pattern(ptr, nullptr);
    rc = pcre2_match(ptr, data, len, 0, 0, md, nullptr);

    g.clear();

    auto ret = rc > 0;

    if (likely(ret)) {
        size_t named = 0;
        pcre2_pattern_info(ptr, PCRE2_INFO_NAMECOUNT, &named);
        if (likely(named > 0)) {
            PCRE2_SPTR tabptr, namedTable;
            uint32_t groupNameSize;
            auto oVector = pcre2_get_ovector_pointer(md);

            pcre2_pattern_info(ptr, PCRE2_INFO_NAMETABLE, &namedTable);
            pcre2_pattern_info(ptr, PCRE2_INFO_NAMEENTRYSIZE, &groupNameSize);

            tabptr = namedTable;
            for (size_t i = 0; i < named; i++) {
                int n = (tabptr[0] << 8) | tabptr[1];
                std::string name((char*)tabptr + 2);
                std::string value(in + oVector[2 * n], (int)(oVector[2 * n + 1] - oVector[2 * n]));

                g.emplace(std::move(name), std::move(value));
                tabptr += groupNameSize;
            }
        }
    }

    pcre2_match_data_free(md);
    return ret;
}

bool Regex::match(const std::string& in, Group& g) const
{
    return match(in.c_str(), in.length(), g);
}

bool Regex::match(const char* in, size_t len) const
{
    auto data = reinterpret_cast<PCRE2_SPTR>(in);
    auto ptr = reinterpret_cast<pcre2_code*>(re_);
    int rc = 0;
    auto md = pcre2_match_data_create_from_pattern(ptr, nullptr);
    rc = pcre2_match(ptr, data, len, 0, 0, md, nullptr);

    pcre2_match_data_free(md);
    return rc > 0;
}

bool Regex::match(const char* in) const
{
    auto ptr = reinterpret_cast<pcre2_code*>(re_);
    if (unlikely(ptr == nullptr || in == nullptr)) {
        return false;
    }
    auto len = strlen(in);

    return match(in, len);
}

bool Regex::match(const std::string& in) const
{
    auto ptr = reinterpret_cast<pcre2_code*>(re_);
    if (unlikely(ptr == nullptr)) {
        return false;
    }
    return match(in.c_str(), in.length());
}

bool Regex::match(const char* begin, const char* end) const
{
    auto ptr = reinterpret_cast<pcre2_code*>(re_);
    if (unlikely(ptr == nullptr || begin == nullptr || end == nullptr || begin > end)) {
        return false;
    }
    return match(begin, end - begin);
}

Regex::~Regex()
{
    auto ptr = reinterpret_cast<pcre2_code*>(re_);
    if (ptr) {
        pcre2_code_free(ptr);
    }
    if (pattern_) {
        delete[] pattern_;
    }
}

#endif

CPPMHD_NAMESPACE_END