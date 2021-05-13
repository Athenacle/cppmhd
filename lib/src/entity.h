#ifndef CPPMHD_INTERNAL_ENTITIY_H_
#define CPPMHD_INTERNAL_ENTITIY_H_

#include <cppmhd/entity.h>

#include <microhttpd.h>

#include "core.h"

CPPMHD_NAMESPACE_BEGIN

enum class RequestState { INITIAL, INITIAL_COMPLETE, DATA_RECEIVING, DATA_RECEIVED, ERROR };

class MHDHttpRequest : public HttpRequest
{
    // MHD_Connection* conn;

    const char* uri;

    HttpMethod mhd;

    RequestState state_;

    std::map<std::string, std::string> param_;

  public:
    MHDHttpRequest(MHD_Connection* con, const char* uri, HttpMethod method);

    std::shared_ptr<DataProcessor> processor()
    {
        return HttpRequest::processor();
    }

    RequestState& state()
    {
        return state_;
    }

    const std::map<std::string, std::string>& param() const
    {
        return param_;
    }

    virtual const std::string& getParam(const std::string& name) const override;

    std::map<std::string, std::string>& param()
    {
        return param_;
    }

    virtual HttpMethod getMethod() const override
    {
        return mhd;
    }

    virtual const char* getHeader(const char* header) const override;

    virtual const char* getPath() const override
    {
        return uri;
    }

    virtual ~MHDHttpRequest() {}
};

CPPMHD_NAMESPACE_END

#endif