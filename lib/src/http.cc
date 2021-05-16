#include "http.h"

#include "config.h"

#include <cppmhd/app.h>
#include <cppmhd/entity.h>

#include <signal.h>

#define FORMAT_INETADDRESS
#define FORMAT_REQUEST_STATE

#ifndef NDEBUG
#define FORMAT_HTTP_METHOD
#endif

#include "entity.h"
#include "format.h"
#include "logger.h"

using namespace cppmhd;
using fmt::format;
using std::move;

namespace
{
bool parseHttpMethod(HttpMethod &out, const char *mth)
{
#define MATCH_METHOD(a, b, like) (like(strncmp(a, b, (sizeof(b) - 1)) == 0))
#define IF_BLOCK_RAW(name, like)                           \
    if (MATCH_METHOD(mth, MHD_HTTP_METHOD_##name, like)) { \
        out = HttpMethod::name;                            \
    }
#define IF_BLOCK(name) IF_BLOCK_RAW(name, unlikely)
#define IF_LIKELY(name) IF_BLOCK_RAW(name, likely)

    bool ret = true;

    // clang-format off
    IF_LIKELY(GET)
    else IF_LIKELY(POST) 
    else IF_BLOCK(PUT) 
    else IF_BLOCK(DELETE) 
    else IF_BLOCK(OPTIONS) 

    else IF_BLOCK(HEAD) 
    else IF_BLOCK(TRACE) 
    else IF_BLOCK(PATCH) 
    else IF_BLOCK(CONNECT) 
    else ret = false;
    // clang-format on

#undef IF_BLOCK
#undef IF_LIKELY
#undef MATCH_METHOD

    return ret;
}

MHD_Return MHDAcceptCB(MAYBE_UNUSED void *cls, MAYBE_UNUSED const struct sockaddr *addr, MAYBE_UNUSED socklen_t addrlen)
{
    return MHD_OK;
}

struct ConnectionObject {
    HttpRequestPtr request;
    HttpResponsePtr response;
    MHDHttpRequest *raw;
    HttpController *ctrl;

#ifndef NDEBUG
    size_t time;
#endif
    template <class... Args>
    ConnectionObject(Args &&...args)
    {
        raw = new MHDHttpRequest(std::forward<Args>(args)...);
        request = HttpRequestPtr(raw);
        ctrl = nullptr;
#ifndef NDEBUG
        time = 1;
#endif
    }
};
}  // namespace

#ifndef NDEBUG
namespace fmt
{
template <>
struct formatter<ConnectionObject> {
    char pres;

    FMT_CONSTEXPR auto parse(format_parse_context &ctx) -> decltype(ctx.begin())
    {
        auto it = ctx.begin();
        return it;
    }

    template <typename FormatContext>
    auto format(const ConnectionObject &p, FormatContext &ctx) -> decltype(ctx.out())
    {
        void *pp = p.raw->processor().get();
        void *resp = p.response.get();
        int status = resp != nullptr ? p.response->status() : 0;
        return format_to(ctx.out(),
                         "<#{}, {}->{}, PP: {}, Request Status: '{}', Response Status: '{}:{}'>",
                         p.time,
                         p.raw->getMethod(),
                         p.raw->getPath(),
                         pp,
                         p.raw->state(),
                         status,
                         dispatchErrorCode(static_cast<HttpStatusCode>(status)));
    }
};
}  // namespace fmt
#endif

namespace
{
void setResponseHeader(MHD_Response *resp, const HttpResponse::HeaderType &headers)
{
    assert(resp);
    for (auto &h : headers) {
        if (MHD_OK != MHD_add_response_header(resp, h.first.c_str(), h.second.c_str())) {
            LOG_WARN("setResponseHeader {}->{} failed", h.first, h.second);
        }
    }
}

MHD_Response *createResponse(HttpResponsePtr &resp, HttpImplement *http)
{
    assert(resp);
    static const std::string oct(CPPMHD_HTTP_MIME_APPLICATION_OCTET);
    auto &body = resp->body();
    auto size = std::get<0>(body);
    auto data = std::get<1>(body);
    auto persis = std::get<2>(body);


    auto res = MHD_create_response_from_buffer(
        size, const_cast<void *>(data), !persis ? MHD_RESPMEM_PERSISTENT : MHD_RESPMEM_MUST_COPY);

    if (size > 0) {
        auto &type = resp->header(CPPMHD_HTTP_HEADER_CONTENT_TYPE);

        if (type.length() == 0) {
            type = oct;
        }
    }
    http->addSharedHeaders(resp);
    setResponseHeader(res, resp->headers());

    return res;
}

MHD_Return sendHttpResponsePtr(MHD_Connection *conn, HttpImplement *http, HttpResponsePtr &resp)
{
    auto res = createResponse(resp, http);
    auto ret = MHD_queue_response(conn, resp->status(), res);
    MHD_destroy_response(res);

    return ret;
}

MHD_Return sendTSR(MHD_Connection *conn, HttpImplement *http, ConnectionObject *obj)
{
    auto next = FORMAT("{}/", obj->request->getPath());
    auto resp = obj->response = http->getErrorHandler()(
        obj->request, k301MovePermanently, HttpError::TSR_FOUND, FORMAT("<a href='{}'>Redirecting...</a>", next));

    resp->header(CPPMHD_HTTP_HEADER_LOCATION, move(next));

    return sendHttpResponsePtr(conn, http, resp);
}

MHD_Return MHDconnectionCB(void *cls,
                           MHD_Connection *conn,
                           const char *url,
                           const char *method,
                           const char *version,
                           const char *data,
                           size_t *dataSize,
                           void **con_cls)
{
    auto http = reinterpret_cast<HttpImplement *>(cls);
    assert(http != nullptr);

    auto co = reinterpret_cast<ConnectionObject *>(*con_cls);


#ifndef NDEBUG
    if (co) {
        co->time++;
        LOG_DTRACE("{}: data {}, size {}", *co, (void *)data, *dataSize);
    } else {
        LOG_DTRACE("{}->{}. data {}, size {}, co {}", method, url, (void *)data, *dataSize, nullptr);
    }
#endif

    if (likely(co == nullptr)) {
        // first round
        bool tsr;
        HttpMethod mtd;

        if (unlikely(!parseHttpMethod(mtd, method))) {
            auto resp = http->getErrorHandler()(nullptr,
                                                k405MethodNotAllowed,
                                                HttpError::BAD_HTTP_METHOD,
                                                FORMAT("un-acceptable Http Method: {}", method));
            LOG_DTRACE("unknown HttpMethod '{}', return 405", method);
            return sendHttpResponsePtr(conn, http, resp);
        }

        *con_cls = co = new ConnectionObject(conn, url, mtd);

        co->response = http->checkRequest(co->request, version);

        if (co->response) {
            LOG_DTRACE("{}: checkRequest return a Response. ", *co);
            return sendHttpResponsePtr(conn, http, co->response);
        }

        co->ctrl = http->forward(co->raw, co->raw->param(), tsr);

        if (likely(co->ctrl)) {
            LOG_DTRACE("{}: route found.", *co);
            co->ctrl->onConnection(co->request, co->response);
            if (co->response) {
                LOG_DTRACE("{}: onConnection handler return a Response", *co);
                return sendHttpResponsePtr(conn, http, co->response);
            }

            LOG_DTRACE("{}: onConnection finished", *co);
            co->raw->state() = RequestState::INITIAL_COMPLETE;
            return MHD_OK;
        } else if (tsr) {
            LOG_DTRACE("{}: TSR found.", *co);
            return sendTSR(conn, http, co);
        } else {
            LOG_DTRACE("{}: no route found, 404 ", *co);

            co->response = http->getErrorHandler()(
                co->request, k404NotFound, HttpError::ROUTER_NOT_FOUND, format("{} to {} not found", method, url));

            return sendHttpResponsePtr(conn, http, co->response);
        }
    }

    auto &state = co->raw->state();

    if (unlikely(state == RequestState::RS_ERROR)) {
        LOG_DTRACE("{}: error recovery: data {}, size {}", *co, (void *)data, *dataSize);
        if (*dataSize != 0) {
            *dataSize = 0;
            return MHD_OK;
        } else {
            return sendHttpResponsePtr(conn, http, co->response);
        }
    }

    assert(state == RequestState::INITIAL_COMPLETE || state == RequestState::DATA_RECEIVED
           || state == RequestState::DATA_RECEIVING);

    if (likely(data == nullptr && *dataSize == 0)) {
        LOG_DTRACE("{}: no data remain ", *co);

        if (unlikely(state == RequestState::DATA_RECEIVING)) {
            auto pp = co->raw->processor();
            assert(pp);
            pp->onData(co->request, nullptr, 0);
            state = RequestState::DATA_RECEIVED;
            LOG_DTRACE("{}: send last data signal to handler.", *co);
            return MHD_OK;
        }

        if (co->ctrl != nullptr) {
            co->ctrl->onRequest(co->request, co->response);
        }

        if (co->response) {
            LOG_DTRACE("{}: request finish", *co);
            return sendHttpResponsePtr(conn, http, co->response);
        } else {
            LOG_DTRACE("{} Handler Should make response but there exist none.", *co);
        }
    } else if (unlikely(co->raw->getMethod() == HttpMethod::GET)) {
        // data in GET Request
        LOG_DTRACE("{}: data in GET, 400", *co);

        co->response = http->getErrorHandler()(
            co->request, k400BadRequest, HttpError::DATA_IN_GET_REQUEST, format("Data in GET Request to {}", url));
        state = RequestState::RS_ERROR;
        *dataSize = 0;
    } else {
        auto pp = co->raw->processor();

        if (unlikely(pp == nullptr)) {
            LOG_DTRACE("{}: PP not found return 405.", *co);
            co->response = http->getErrorHandler()(co->request,
                                                   k405MethodNotAllowed,
                                                   HttpError::DATA_PROCESSOR_NOT_SET,
                                                   format("Data Processor not set in {} Request to {}", method, url));

            state = RequestState::RS_ERROR;
            *dataSize = 0;

        } else {
            if (unlikely(state == RequestState::INITIAL_COMPLETE)) {
                LOG_DTRACE("{}: PP found", *co);
                state = RequestState::DATA_RECEIVING;
            }

            size_t size = pp->onData(co->request, data, *dataSize);
            LOG_DTRACE("{}: PP process {}, in {} bytes, return {} bytes", *co, (void *)data, *dataSize, size);

            if (unlikely(size == DataProcessor::DataProcessorParseFailed)) {
                co->response =
                    http->getErrorHandler()(co->request,
                                            k400BadRequest,
                                            HttpError::DATA_PROCESSOR_RETURN_ERROR,
                                            format("Data Processor return an error in {} Request to {}", method, url));

                state = RequestState::RS_ERROR;
                size = *dataSize;
            } else if (unlikely(size > *dataSize)) {
                LOG_WARN(
                    "Data Processor return read size {} larger than original data size {}. "
                    "This must be a programming error.",
                    size,
                    *dataSize);
                size = *dataSize;
            }

            *dataSize -= size;
        }
    }
    return MHD_OK;
}

void connectionFinishCB(void *cls,
                        MAYBE_UNUSED MHD_Connection *conn,
                        void **data,
                        MAYBE_UNUSED MHD_RequestTerminationCode toe)
{
    auto http = reinterpret_cast<HttpImplement *>(cls);
    auto req = reinterpret_cast<ConnectionObject *>(*data);
    if (req != nullptr) {
        delete req;
    }

    if (http->isLogConnectionStatus()) {
        static const char why[][20] = {
            "Successful Complete", "With Error", "Timeout", "Daemon Shutdown", "Read Error", "Client Abort"};
        LOG_TRACE("Connection Finished: {}", why[toe]);
    }
}

void notifyConnectionCB(void *cls,
                        MHD_Connection *conn,
                        MAYBE_UNUSED void **socket_context,
                        MHD_ConnectionNotificationCode toe)
{
    auto http = reinterpret_cast<HttpImplement *>(cls);

    if (http->isLogConnectionStatus()) {
        auto coninfo = MHD_get_connection_info(conn, MHD_CONNECTION_INFO_CLIENT_ADDRESS);

        if (likely(coninfo && coninfo->client_addr)) {
            static const char code[][8] = {"Open", "Close"};
            auto fdinfo = MHD_get_connection_info(conn, MHD_CONNECTION_INFO_CONNECTION_FD);
            auto fd = fdinfo == nullptr ? 0 : fdinfo->connect_fd;

            if (unlikely(http->isV6())) {
                sockaddr_in6 &addr = *(reinterpret_cast<sockaddr_in6 *>(coninfo->client_addr));
                LOG_TRACE("{} connection to {} (#{})", code[toe], addr, fd);
            } else {
                sockaddr_in &addr = *(reinterpret_cast<sockaddr_in *>(coninfo->client_addr));
                LOG_TRACE("{} connection to {} (#{})", code[toe], addr, fd);
            }
        }
    }
}

void *MHD_URI_LOGGER(void *, const char *, struct MHD_Connection *)
{
    return nullptr;
}

void MHD_LOGGER(void *, const char *fmt, va_list ap)
{
    const size_t size = 512;
    char buffer[size];
    int len = vsnprintf(buffer, size, fmt, ap);
    if (likely(len > 0 && buffer[len - 1] == '\n')) {
        buffer[len - 1] = 0;
    }
    LOG_INFO("{}", buffer);
}


void startWait(Barrier *b)
{
    b->wait();
}


uint32_t calcFlag()
{
    auto flag = MHD_USE_SUPPRESS_DATE_NO_CLOCK | MHD_USE_TURBO;
#ifndef NDEBUG
    flag |= MHD_USE_DEBUG;
#endif

    if (MHD_is_feature_supported(MHD_FEATURE_TCP_FASTOPEN) == MHD_OK) {
        flag |= MHD_USE_TCP_FASTOPEN;
    }

    if (MHD_is_feature_supported(MHD_FEATURE_EPOLL) == MHD_OK) {
        flag |= MHD_USE_EPOLL_INTERNAL_THREAD;
    } else if (MHD_is_feature_supported(MHD_FEATURE_POLL) == MHD_OK) {
        flag |= MHD_USE_POLL_INTERNAL_THREAD;
    } else {
        flag |= MHD_USE_AUTO_INTERNAL_THREAD;
    }
    return flag;
}
}  // namespace

void HttpImplement::stop()
{
    if (isRunning()) {
        runningBarrier_.wait();
    }
}

CPPMHD_Error HttpImplement::startMHDDaemon(uint32_t tc,
                                           const std::function<void(void)> &cb,
                                           const std::vector<int> &sigs)
{
    auto flag = calcFlag();
    if (addr.isV6()) {
        flag |= MHD_USE_DUAL_STACK;
    }

    if (tc > getNProc() << 3) {
        auto next = getNProc() << 2;
        LOG_WARN("Too large threadCount {}. Set to {}.", tc, next);
        tc = next;
    }

    auto sock = addr.getSocket();

    MHD_OptionItem ops[] = {
        {MHD_OPTION_EXTERNAL_LOGGER, (intptr_t)MHD_LOGGER, this},
        {MHD_OPTION_NOTIFY_COMPLETED, (intptr_t)connectionFinishCB, this},
        {MHD_OPTION_NOTIFY_CONNECTION, (intptr_t)notifyConnectionCB, this},
        {MHD_OPTION_STRICT_FOR_CLIENT, 0, nullptr},
#if MHD_VERSION >= 0x96800L
        {MHD_OPTION_SERVER_INSANITY, MHD_DSC_SANE, nullptr},
#endif
        {MHD_OPTION_SOCK_ADDR, 0, (void *)sock},
        {MHD_OPTION_URI_LOG_CALLBACK, (intptr_t)MHD_URI_LOGGER, this},
        {MHD_OPTION_LISTENING_ADDRESS_REUSE, tc == 1 ? 0 : 1, nullptr},
        {MHD_OPTION_END, 0, nullptr}
    };

    {
        std::lock_guard<std::mutex> _(global::mutex);
        for (auto i = 0u; i < tc; i++) {
            auto d = MHD_start_daemon(
                flag, addr.port(), MHDAcceptCB, this, MHDconnectionCB, this, MHD_OPTION_ARRAY, ops, MHD_OPTION_END);
            if (d != nullptr) {
                LOG_INFO("#{}: begin listening at {}", i, addr);
                daemons.emplace_back(d);
            } else {
                LOG_ERROR("#{}: listen failed: {}, stop running threads", i, strerror(errno));

                for (auto &daemon : daemons) {
                    MHD_stop_daemon(daemon);
                }

                return CPPMHD_Error::CPPMHD_LISTEN_FAILED;
            }
        }
    }
#ifdef HAVE_PTHREAD_SIGMASK
    {
        sigset_t sigset;
        sigemptyset(&sigset);
        for (auto i : sigs) {
            sigaddset(&sigset, i);
        }
        auto mask = pthread_sigmask(SIG_SETMASK, &sigset, nullptr);
        if (mask) {
            LOG_ERROR("pthread_sigmask failed: {}", mask);
        }
    }
#endif

    running = true;
    thr = std::thread(startWait, &runningBarrier_);
    cb();
    thr.join();
    running = false;
    {
        std::lock_guard<std::mutex> _(global::mutex);
        LOG_INFO("{}", "stopping MHD daemon...");

        for (auto &d : daemons) {
            MHD_stop_daemon(d);
        }
    }
    return CPPMHD_Error::CPPMHD_OK;
}

HttpResponsePtr HttpImplement::checkRequest(const HttpRequestPtr &req, const char *version)
{
    if (host.length() != 0) {
        auto h = req->getHeader(CPPMHD_HTTP_HEADER_HOST);
        if (h == nullptr || strcmp(h, host.c_str()) != 0) {
            return getErrorHandler()(
                req, k400BadRequest, HttpError::HOST_FIELD_INCORRECT, "Host Field Not Set Correctly");
        }
    }

    if (strcmp(version, MHD_HTTP_VERSION_1_1) != 0) {
        return getErrorHandler()(req,
                                 k505HttpVersionNotSupported,
                                 HttpError::BAD_HTTP_VERSION,
                                 format("Bad HTTP Version {}. HTTP1/1 is the only acceptable version", version));
    }

    return nullptr;
}

void HttpImplement::addSharedHeaders(HttpResponsePtr &resp)
{
    auto &server = resp->header(CPPMHD_HTTP_HEADER_SERVER);
    if (server.length() == 0) {
        server = PROJECT_SERVER_HEADER;
    }
}