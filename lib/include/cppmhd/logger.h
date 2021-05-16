
#ifndef CPPMHD_LOGGER_H_
#define CPPMHD_LOGGER_H_

#include <cppmhd/core.h>
#include <memory>

CPPMHD_NAMESPACE_BEGIN

namespace log
{
enum LogLevel
{
    kTrace = 0,
    kDebug,
    kInfo,
    kWarning,
    kError,
    kFatal
};

class AbstractLogger
{
  public:
    virtual void output(enum LogLevel lv,
                        const char* func,
                        const char* file,
                        int line,
                        const char* message) = 0;

    virtual void fflush() = 0;
};

void setLogger(std::shared_ptr<AbstractLogger> logger);

template <class L, class... Args>
std::shared_ptr<L> setLogger(Args&&... args)
{
    auto l = std::make_shared<L>(std::forward<Args>(args)...);
    setLogger(l);
    return l;
}

void setLogLevel(enum LogLevel);
}  // namespace log

CPPMHD_NAMESPACE_END

#endif
