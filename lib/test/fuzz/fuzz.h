#ifndef CPPMHD_INTERNAL_FUZZ_H_
#define CPPMHD_INTERNAL_FUZZ_H_

#include <cppmhd/controller.h>
#include <cppmhd/entity.h>

#include "logger.h"

namespace
{
class NoneLogger : public cppmhd::log::AbstractLogger
{
  public:
    virtual void output(enum cppmhd::log::LogLevel, const char*, const char*, int, const char*) override{};

    virtual void fflush() override {}
    virtual ~NoneLogger() {}
};

class LoggerRegister
{
    static LoggerRegister reg;

  public:
    LoggerRegister()
    {
        cppmhd::log::setLevel(cppmhd::log::LogLevel::kFatal);
        cppmhd::log::setLogger<NoneLogger>();
    }
};
LoggerRegister LoggerRegister::reg;

class Ctrl : public cppmhd::HttpController
{
    virtual void onRequest(cppmhd::HttpRequestPtr, cppmhd::HttpResponsePtr&) override {}
};
}  // namespace

#endif