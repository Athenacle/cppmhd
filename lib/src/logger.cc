#include "logger.h"

#include "config.h"

#include <fmt/chrono.h>
#include <time.h>

#include <atomic>
#include <cstdio>
#include <mutex>

using namespace cppmhd::log;

#define RED "\033[31m"
#define FATAL "\033[1m" RED
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define DIM " \033[2m"
#define LG "\033[37m"

#define RESET "\033[0m"

CPPMHD_NAMESPACE_BEGIN

namespace log
{
void SimpleSTDOUTLogger::output(enum LogLevel lv, const char*, const char* func, int line, const char* message)

{
    static const char* lvStr[16] = {MAGENTA " [TRACE] " RESET,
                                    BLUE " [DEBUG] " RESET,
                                    GREEN " [INFO] " RESET,
                                    YELLOW " [WARN] " RESET,
                                    RED " [ERROR] " RESET,
                                    FATAL " [FATAL] " RESET};

    if (lv >= getLevel()) {
        auto level = lv;

        if (level > kError) {
            WARN("invalid LogLevel {}. Assume as kInfo", level);

            level = kInfo;
        }

        auto all = fmt::format(
            "{:%Y-%m-%d %H:%M:%S} {} [{}:{}] {}\n", fmt::localtime(time(nullptr)), lvStr[level], func, line, message);

        mutex.lock();
        fprintf(stdout, "%s", all.c_str());
        mutex.unlock();
    }
}

void SimpleSTDOUTLogger::fflush()
{
    std::fflush(stdout);
}

SimpleSTDOUTLogger::~SimpleSTDOUTLogger() {}

std::atomic<uint8_t> level
#ifndef NDEBUG
    (LogLevel::kTrace);
#else
    (LogLevel::kInfo);
#endif

std::shared_ptr<AbstractLogger> logInstance = std::make_shared<SimpleSTDOUTLogger>();

void setLogger(std::shared_ptr<AbstractLogger> logger)
{
    logInstance = logger;
}

void setLogLevel(enum LogLevel lv)
{
    if (lv >= kTrace && lv <= kError) {
        setLevel(lv);
    }
}

}  // namespace log
CPPMHD_NAMESPACE_END