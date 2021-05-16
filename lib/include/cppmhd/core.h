#ifndef CPPMHD_CORE_H_
#define CPPMHD_CORE_H_

#include <memory>

struct MHD_Connection;  // for  microhttpd

#define CPPMHD_NAMESPACE_BEGIN \
    namespace cppmhd           \
    {
CPPMHD_NAMESPACE_BEGIN

class App;
class Router;
class RouterBuilder;
class HttpRequest;
class HttpResponse;


class DataProcessor;

class HttpImplement;
class HttpConnection;

enum class HttpMethod { GET, HEAD, POST, PUT, DELETE, OPTIONS, CONNECT, TRACE, PATCH };

enum HttpStatusCode {
    // Information responses
    k100Continue = 100,
    k101SwitchingProtocol,
    k102Processing,
    k103EarlyHints,

    // Successful responses
    k200OK = 200,
    k201Created,
    k202Accepted,
    k203NonAuthoritativeInfomation,
    k204NoContent,
    k205ResetContent,
    k206PartialContent,

    /*
     * Reserved
     * k207MultiStatus,
     * k208AlreadyReported,
     * k226IMUsed = 226
     */

    // Redirection messages
    k300MultipleChoice = 300,
    k301MovePermanently,
    k302Found,
    k303SeeOther,
    k304NotModified,
    k305UseProxy,

    k307TemporaryRedirect = 307,
    k308PermanentRedirect = 308,

    // Client error responses
    k400BadRequest = 400,
    k401Unauthorized,
    k402PaymentRequired,
    k403Forbidden,
    k404NotFound,
    k405MethodNotAllowed,
    k406NoAcceptable,
    k407ProxyAuthenticationRequired,
    k408RequestTimeout,
    k409Conflict,
    k410Gone,
    k411LengthRequired,
    k412PreconditionFailed,
    k413PayloadTooLarge,
    k414URITooLong,
    k415UnsupportedMediaType,
    k416RangeNotSatisfiable,
    k417ExpectationFailed,
    k418ImaTeapot,
    k421MisDirectedRequest = 421,
    k429TooManyRequests = 429,

    // Server error responses
    k500InternalServerError = 500,
    k501NotImplemented,
    k502BadGateway,
    k503ServiceUnavailable,
    k504GatewayTimeout,
    k505HttpVersionNotSupported,
    k506VariantAlsoNegotiates,

};

void* cpp_mhd_malloc(unsigned long);

void cpp_mhd_free(void*);

void cpp_mhd_set_malloc(void*(mallocF)(unsigned long), void (*freeF)(void*));

template <class T, class... Args>
T* cppmhd_construct(Args&&... args)
{
    auto* ptr = cpp_mhd_malloc(sizeof(T));
    return new (ptr) T(std::forward<Args>(args)...);
}

template <class T>
void cppmhd_destruct(T* ptr)
{
    if (ptr) {
        ptr->~T();
        cpp_mhd_free(ptr);
    }
}

template <class T>
struct cppmhd_delete {
    void operator()(T* ptr) const
    {
        if (ptr) {
            ptr->~T();
            cpp_mhd_free(ptr);
        }
    }
};


#define CPPMHD_HTTP_HEADER_ACCEPT_CHARSET "Accept-Charset"
#define CPPMHD_HTTP_HEADER_ACCEPT_ENCODING "Accept-Encoding"
#define CPPMHD_HTTP_HEADER_ACCEPT_LANGUAGE "Accept-Language"

#define CPPMHD_HTTP_HEADER_CONTENT_LENGTH "Content-Length"
#define CPPMHD_HTTP_HEADER_CONTENT_LOCATION "Content-Location"
#define CPPMHD_HTTP_HEADER_CONTENT_TYPE "Content-Type"

#define CPPMHD_HTTP_HEADER_HOST "Host"
#define CPPMHD_HTTP_HEADER_LOCATION "Location"

#define CPPMHD_HTTP_HEADER_SERVER "Server"

#define CPPMHD_HTTP_MIME_APPLICATION_JSON "application/json"
#define CPPMHD_HTTP_MIME_APPLICATION_OCTET "application/octet-stream"
#define CPPMHD_HTTP_MIME_APPLICATION_XML "application/xml"
#define CPPMHD_HTTP_MIME_APPLICATION_FORM_URLENCODED "application/x-www-form-urlencoded"
#define CPPMHD_HTTP_MIME_MULTIPART_FORM_DATA "multipart/form-data"
#define CPPMHD_HTTP_MIME_TEXT_HTML "text/html"
#define CPPMHD_HTTP_MIME_TEXT_PLAIN "text/plain"

#define CPPMHD_NAMESPACE_END }

CPPMHD_NAMESPACE_END

#endif