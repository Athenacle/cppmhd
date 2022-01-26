#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "logger.h"
#include "router.h"
#include "utils.h"

using namespace cppmhd;
using namespace std;
using namespace testing;


using lv = log::LogLevel;
using s = const char*;

class MockLogger : public log::AbstractLogger
{
  public:
    MOCK_METHOD((void), output, (lv, s, s, int, s), (override));

    MOCK_METHOD((void), fflush, (), (override));

    virtual ~MockLogger() {}
};

class BadRouter : public Test
{
    uint8_t lv;

  protected:
    std::shared_ptr<MockLogger> mock;

  public:
    void SetUp()
    {
        lv = log::getLevel();
        log::fflush();
        mock = log::setLogger<MockLogger>();
        log::setLogLevel(log::LogLevel::kError);
    }

    void TearDown()
    {
        log::unSetLogger();
        log::setLevel(static_cast<log::LogLevel>(lv));
        log::fflush();
    }
};

class MockOutput : public ActionInterface<void(log::LogLevel, s, s, int, s)>
{
    std::vector<Regex> res;
    log::LogLevel level_;

  public:
    void Perform(const std::tuple<log::LogLevel, s, s, int, s>& args)
    {
        auto level = std::get<0>(args);
        auto msg = std::get<4>(args);
        ASSERT_EQ(level, log::LogLevel::kError);

        bool status = false;
        for (const auto& re : res) {
            status = status || re.match(msg);
        }
        ASSERT_TRUE(status);
        std::cout << msg << std::endl;
    }


    void add() {}

    template <class... Args>
    void add(log::LogLevel lv_, Args&&... args)
    {
        level_ = lv_;
        add(std::forward<Args>(args)...);
    }

    template <class... Args>
    void add(const char* path, Args&&... args)
    {
        Regex next;
        ASSERT_TRUE(Regex::compileRegex(next, path));
        res.emplace_back(std::move(next));

        add(std::forward<Args>(args)...);
    }

    template <class... Args>
    void init(Args&&... args)
    {
        add(std::forward<Args>(args)...);
    }

    template <class... Args>
    explicit MockOutput(Args&&... args)
    {
        level_ = log::LogLevel::kError;
        init(std::forward<Args>(args)...);
    }
};

auto ef = [](HttpRequestPtr) -> HttpResponsePtr { return nullptr; };

#define MOCK_PATTERN_RAW(times, ...)                                                              \
    do {                                                                                          \
        MockLogger& l = *mock;                                                                    \
        ON_CALL(l, output(_, _, _, _, _)).WillByDefault(MakeAction(new MockOutput(__VA_ARGS__))); \
        EXPECT_CALL(l, output(_, _, _, _, _)).Times(times);                                       \
    } while (false)

#define MOCK_PATTERN(...) MOCK_PATTERN_RAW(1, __VA_ARGS__)

TEST_F(BadRouter, handlerExists)
{
    RouterBuilder rb;

    MOCK_PATTERN("^Handle to .*? already exists.$");

    rb.add(HttpMethod::GET, "/foo/bar", ef);
    rb.add(HttpMethod::GET, "/foo/bar", ef);

    Router r(move(rb));
    ASSERT_FALSE(r.build());
}

TEST_F(BadRouter, badRoutePattern)
{
    RouterBuilder rb;

    MOCK_PATTERN("^Bad Route Patte.*?skip$");

    auto ctrl = rb.add(HttpMethod::GET, "**////**/J//**///**/t", ef);

    HttpResponsePtr resp;
    ctrl->onRequest(nullptr, resp);  // just increase code coverage

    Router r(move(rb));

    ASSERT_FALSE(r.build());
}

TEST_F(BadRouter, unknownSegment)
{
    RouterBuilder rb;
    auto pat = "Unknown URI segment .*?, skip...";
    MOCK_PATTERN(pat);

    rb.add(HttpMethod::GET, "/foo/{bad-segment", ef);

    Router r(move(rb));
    ASSERT_FALSE(r.build());
}

TEST_F(BadRouter, WildcardConflict)
{
    RouterBuilder rb;

    MOCK_PATTERN("^(w|W)ildcard conflict .*$");


    rb.add(HttpMethod::GET, "/foo/{:id}", ef);
    rb.add(HttpMethod::GET, "/foo/123", ef);

    Router r(move(rb));
    ASSERT_FALSE(r.build());
}

TEST_F(BadRouter, regexPatternFailed)
{
    RouterBuilder rb;
    auto pat = "Compile regex pattern '.*?' in path '.*?' failed. skip...";
    auto pat2 = "compile regex pattern '.*?' failed at offset \\d";
    MOCK_PATTERN_RAW(2, pat, pat2);

    rb.add(HttpMethod::GET, "/foo/{*|:id}", ef);

    Router r(move(rb));
    ASSERT_FALSE(r.build());
}

TEST_F(BadRouter, WildcardSegmentConflictsWith)
{
    RouterBuilder rb;

    auto pat = "^Wildcard segment .*? conflicts with existing child in path .*?$";

    MOCK_PATTERN(pat);


    rb.add(HttpMethod::GET, "/foo/123", ef);
    rb.add(HttpMethod::GET, "/foo/{:id}", ef);

    Router r(move(rb));
    ASSERT_FALSE(r.build());
}