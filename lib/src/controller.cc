#include "config.h"

#include <cppmhd/controller.h>

#include <cassert>

#include "http.h"
#include "logger.h"
#include "utils.h"

CPPMHD_NAMESPACE_BEGIN

void HttpController::onConnection(HttpRequestPtr, HttpResponsePtr &)
{
    // no-op
}

HttpController::~HttpController() {}

DataProcessor::~DataProcessor() {}

DataProcessor::DataProcessor()
{
    LOG_DTRACE("{} this = {}", "DataProcessor::DataProcessor", (void *)this);
}


namespace
{
using ccp = const char *;

MHD_Return formIter(void *, MHD_ValueKind, ccp, ccp, ccp, ccp, ccp, uint64_t, size_t);

class FormProcessor;
struct FormProcessorData {
    FormProcessor *proc;
    std::weak_ptr<HttpRequest> req;
};

class FormProcessor : public DataProcessor
{
    friend MHD_Return formIter(void *, MHD_ValueKind, ccp, ccp, ccp, ccp, ccp, uint64_t, size_t);

    MHD_Connection *conn_;
    MHD_PostProcessor *pp_;
    FormProcessorData data_;
    FormDataProcessorController *ctrl_;

    std::string kN_, fN_, cT_, tE_;

  public:
    virtual ~FormProcessor()
    {
        if (likely(pp_)) {
            MHD_destroy_post_processor(pp_);
        }
    }

    virtual size_t onData(HttpRequestPtr &req, const void *data, size_t size) override
    {
        assert(pp_);
        if (unlikely(size == 0)) {
            ctrl_->onData(req, kN_, fN_, cT_, tE_, nullptr, 0, 0, true);
            return 0;
        } else {
            if (likely(MHD_post_process(pp_, reinterpret_cast<const char *>(data), size)) == MHD_OK) {
                return size;
            } else {
                return DataProcessor::DataProcessorParseFailed;
            }
        }
    }

    bool buildPostProcessor(HttpRequestPtr req)
    {
        data_.req = req;
        pp_ = MHD_create_post_processor(conn_, 20 * 1024, formIter, &data_);
        return pp_ != nullptr;
    }

    FormProcessor(MHD_Connection *conn, FormDataProcessorController *ctrl) : conn_(conn), pp_(nullptr), ctrl_(ctrl)
    {
        data_.proc = this;
    }

    bool onData(HttpRequestPtr &req, ccp key, ccp fN, ccp cT, ccp tE, ccp data, uint64_t off, size_t size)
    {
        if (key && kN_ != key) {
            kN_ = key;
        }
        if (fN && fN_ != fN) {
            fN_ = fN;
        }
        if (cT && cT_ != cT) {
            cT_ = cT;
        }
        if (tE && tE_ != tE) {
            tE_ = tE;
        }

        return ctrl_->onData(req, kN_, fN_, cT_, tE_, data, off, size, false);
    }
};


MHD_Return formIter(
    void *cls, MAYBE_UNUSED MHD_ValueKind kind, ccp key, ccp fN, ccp cT, ccp tE, ccp data, uint64_t off, size_t size)
{
    MAYBE_UNUSED auto fp = reinterpret_cast<FormProcessorData *>(cls);
    assert(kind == MHD_POSTDATA_KIND);
    auto req = fp->req.lock();
    return fp->proc->onData(req, key, fN, cT, tE, data, off, size) ? MHD_OK : MHD_FAILED;
}

}  // namespace

void FormDataProcessorController::postConnection(HttpRequestPtr) {}

void FormDataProcessorController::onConnection(HttpRequestPtr req, HttpResponsePtr &)
{
    auto conn = req->connection();
    if (likely(conn)) {
        auto ct = req->getHeader(CPPMHD_HTTP_HEADER_CONTENT_TYPE);
        if (isPrefixOfArray(ct, CPPMHD_HTTP_MIME_APPLICATION_FORM_URLENCODED)
            || isPrefixOfArray(ct, CPPMHD_HTTP_MIME_MULTIPART_FORM_DATA)) {
            auto fp = new FormProcessor(conn, this);
            if (fp->buildPostProcessor(req)) {
                pp = std::shared_ptr<DataProcessor>(fp);
                req->setProcessor(pp);
                postConnection(req);
            } else {
                delete fp;
            }
        }
    }
}

FormDataProcessorController::~FormDataProcessorController() {}

CPPMHD_NAMESPACE_END