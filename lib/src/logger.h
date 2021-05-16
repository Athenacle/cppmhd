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
    return level;
}

inline void setLevel(enum LogLevel lv)
{
    level = lv;
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

#define LOG_TRACE(f, ...) LOGIT(cppmhd::log::LogLevel::kTrace, FORMAT(f, ##__VA_ARGS__))
#define LOG_DEBUG(f, ...) LOGIT(cppmhd::log::LogLevel::kDebug, FORMAT(f, ##__VA_ARGS__))
#define LOG_INFO(f, ...) LOGIT(cppmhd::log::LogLevel::kInfo, FORMAT(f, ##__VA_ARGS__))
#define LOG_WARN(f, ...) LOGIT(cppmhd::log::LogLevel::kWarning, FORMAT(f, ##__VA_ARGS__))
#define LOG_ERROR(f, ...) LOGIT(cppmhd::log::LogLevel::kError, FORMAT(f, ##__VA_ARGS__))

#define LV_DTRACE (cppmhd::log::LogLevel::kFatal + 1)

#ifndef NDEBUG
#define LOG_DTRACE(f, ...)                                                                                          \
    do {                                                                                                            \
        cppmhd::log::logInstance->output(                                                                           \
            static_cast<log::LogLevel>(LV_DTRACE), __FILE__, __func__, __LINE__, FORMAT(f, ##__VA_ARGS__).c_str()); \
    } while (false)
#else
#define LOG_DTRACE(f, ...) (void(0))
#endif

}  // namespace log

CPPMHD_NAMESPACE_END

#endif