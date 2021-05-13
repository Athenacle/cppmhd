#ifndef CPPMHD_INTERNAL_LOGGER_H_
#define CPPMHD_INTERNAL_LOGGER_H_
#include "config.h"

#include <cppmhd/logger.h>

#include <fmt/format.h>

#include <atomic>
#include <mutex>

#include "core.h"

CPPMHD_NAMESPACE_BEGIN

namespace log
{
extern std::atomic<uint8_t> level;
extern std::shared_ptr<AbstractLogger> logInstance;

inline uint8_t getLevel()
{
#if __cplusplus >= 201709L
    return level.load(std::memory_order::acquire);
#else
    return level.load(std::memory_order::memory_order_acquire);
#endif
}

inline void setLevel(enum LogLevel lv)
{
#if __cplusplus >= 201709L
    level.store(lv, std::memory_order::release);
#else
    level.store(lv, std::memory_order::memory_order_release);
#endif
}

class SimpleSTDOUTLogger : public AbstractLogger
{
    std::mutex mutex;

  public:
    virtual void output(enum LogLevel lv, const char*, const char* func, int line, const char* message) override;
    virtual void fflush() override;
    virtual ~SimpleSTDOUTLogger();
};

inline void fflush()
{
    logInstance->fflush();
}

#ifdef TESTING
inline void unSetLogger()
{
    logInstance = std::make_shared<SimpleSTDOUTLogger>();
}
#endif

#define LOGIT(lv, msg)                                                                       \
    do {                                                                                     \
        if (cppmhd::log::getLevel() <= lv) {                                                 \
            cppmhd::log::logInstance->output(lv, __FILE__, __func__, __LINE__, msg.c_str()); \
        }                                                                                    \
    } while (false)

#define TRACE(f, ...) LOGIT(cppmhd::log::LogLevel::kTrace, FORMAT(f, ##__VA_ARGS__))
#define DEBUG(f, ...) LOGIT(cppmhd::log::LogLevel::kDebug, FORMAT(f, ##__VA_ARGS__))
#define INFO(f, ...) LOGIT(cppmhd::log::LogLevel::kInfo, FORMAT(f, ##__VA_ARGS__))
#define WARN(f, ...) LOGIT(cppmhd::log::LogLevel::kWarning, FORMAT(f, ##__VA_ARGS__))
#define ERROR(f, ...) LOGIT(cppmhd::log::LogLevel::kError, FORMAT(f, ##__VA_ARGS__))
}  // namespace log

CPPMHD_NAMESPACE_END

#endif