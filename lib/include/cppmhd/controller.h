#ifndef CPPMHD_CONTROLLER_H_
#define CPPMHD_CONTROLLER_H_

#include <cppmhd/core.h>
#include <cppmhd/entity.h>

#include <type_traits>

CPPMHD_NAMESPACE_BEGIN

class DataProcessor
{
  public:
    DataProcessor();
    virtual ~DataProcessor();

    static const size_t DataProcessorParseFailed = static_cast<size_t>(~0);

    virtual size_t onData(HttpRequestPtr& req, const void* in, size_t size) = 0;
};

template <class T>
class DataProcessorHelper
{
    bool operator()(T&, const void*, size_t) const;
};

template <class DT>
class HttpDataProcessor : public DataProcessor
{
    DT* data_;

  public:
    using my_type = HttpDataProcessor<DT>;
    using data_pointer = DT*;
    using data_reference = DT&;
    using data_const_reference = const DT&;

  private:
    virtual size_t onData(HttpRequestPtr& req, data_reference data, const void* in, size_t size) = 0;

    virtual size_t onData(HttpRequestPtr& req, const void* in, size_t size) override final
    {
        return onData(req, *data_, in, size);
    }

  public:
    data_const_reference data() const
    {
        return *data_;
    }
    data_reference data()
    {
        return *data_;
    }
    HttpDataProcessor()
    {
        data_ = cppmhd_construct<DT>();
    }

    virtual ~HttpDataProcessor()
    {
        cppmhd_destruct(data_);
    }
};

class HttpController
{
  public:
    virtual void onConnection(HttpRequestPtr, HttpResponsePtr&);

    virtual void onRequest(HttpRequestPtr, HttpResponsePtr&) = 0;

    virtual ~HttpController();
};

using HttpControllerPtr = std::shared_ptr<HttpController>;

template <class DT, class P = HttpDataProcessor<DT>>
class DataProcessorController : public HttpController
{
    using data_type = const DT&;

    virtual void onConnection(HttpRequestPtr req, HttpResponsePtr&) override final
    {
        std::shared_ptr<DataProcessor> pp;
        auto p = cppmhd_construct<P>();
        pp = std::shared_ptr<DataProcessor>(p, cppmhd_delete<P>());
        req->setProcessor(pp);
    }

    virtual void onRequest(HttpRequestPtr req, data_type data, HttpResponsePtr&) = 0;

    virtual void onRequest(HttpRequestPtr req, HttpResponsePtr& resp) override final
    {
        auto p = req->processor().get();
        return onRequest(req, reinterpret_cast<P*>(p)->data(), resp);
    }

  public:
    virtual ~DataProcessorController() {}

    DataProcessorController() = default;
};

class FormDataProcessorController : public HttpController
{
    std::shared_ptr<DataProcessor> pp;

  public:
    virtual void onConnection(HttpRequestPtr, HttpResponsePtr&) override final;

    virtual bool onData(const std::string& keyName,
                        const std::string& fileName,
                        const std::string& contentType,
                        const std::string& transferEncoding,
                        const void* data,
                        size_t offset,
                        size_t size,
                        bool finish) = 0;

    FormDataProcessorController() = default;

    virtual ~FormDataProcessorController();
};

CPPMHD_NAMESPACE_END

#endif
