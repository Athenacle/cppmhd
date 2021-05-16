#include "logger.h"

#include "config.h"

#include <fmt/chrono.h>
#include <time.h>

#include <atomic>
#include <cstdio>
#include <mutex>

using namespace cppmhd::log;

#ifndef ON_WINDOWS
#define RED "\033[31m"
#define FATAL "\033[1m" RED
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define DIM " \033[2m"
#define LG "\033[37m"
#define DG "\033[90m"
#define RESET "\033[0m"
#else
#define RED 
#define FATAL 
#define GREEN 
#define YELLOW
#define BLUE 
#define MAGENTA 
#define DIM 
#define LG 
#define DG
#define RESET
#endif

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
                                    FATAL " [FATAL] " RESET
#ifndef NDEBUG
                                    ,
                                    DG "[DTRACE]" RESET
#endif
    };
#ifdef NDEBUG
    if (lv >= getLevel()) {
        auto level = lv;
        if (level > kError) {
            LOG_WARN("invalid LogLevel {}. Assume as kInfo", level);

            level = kInfo;
        }
#else
    if (lv >= getLevel() && lv <= LV_DTRACE) {
        auto level = lv;
#endif
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