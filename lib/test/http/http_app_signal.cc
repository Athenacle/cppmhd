#include "config.h"

#ifdef UNIX

#include "http_app.h"
#include "test.h"

using namespace std::placeholders;
using namespace testing;

class SHHandler : public ActionInterface<void(App &, int)>
{
    std::string h;
    int sig;

  public:
    void Perform(const std::tuple<App &, int> &args)
    {
        const auto &app = std::get<0>(args);
        const auto sig = std::get<1>(args);
        EXPECT_EQ(app.host(), h);
        EXPECT_EQ(sig, this->sig);
    }

    SHHandler(std::string &host, int sig) : h(host), sig(sig) {}
    virtual ~SHHandler() {}
};

TEST_F(HttpApp, signal)
{
    SHMock mock;
    std::string hname = myName;
    app->add<TestCtrl>(HttpMethod::GET, myName);
    EXPECT_TRUE(App::setSignalHandler(SIGALRM, std::bind(&SHMock::handler, &mock, _1, _2)));
    app->host() = hname;
    start();

    EXPECT_CALL(mock, handler(_, _)).Times(1);
    ON_CALL(mock, handler(_, _)).WillByDefault(MakeAction(new SHHandler(hname, SIGALRM)));

    raise(SIGALRM);
}

TEST_F(HttpApp, registerSignalFailed)
{
    SHMock mock;
    ASSERT_FALSE(App::setSignalHandler(-1, std::bind(&SHMock::handler, &mock, _1, _2)));
    EXPECT_CALL(mock, handler(_, _)).Times(0);
}

TEST_F(HttpApp, signalRegisterTwice)
{
    SHMock mock, mock1;
    std::string hname = myName;
    app->add<TestCtrl>(HttpMethod::GET, myName);
    ASSERT_TRUE(App::setSignalHandler(SIGUSR1, std::bind(&SHMock::handler, &mock, _1, _2)));
    ASSERT_TRUE(App::setSignalHandler(SIGUSR1, std::bind(&SHMock::handler, &mock1, _1, _2)));
    app->host() = hname;
    start();

    EXPECT_CALL(mock, handler(_, _)).Times(0);
    EXPECT_CALL(mock1, handler(_, _)).Times(0);
    ON_CALL(mock, handler(_, _)).WillByDefault(MakeAction(new SHHandler(hname, SIGUSR1)));
}

TEST_F(HttpApp, selfSigint)
{
    app->add<TestCtrl>(HttpMethod::GET, myName);
    start();

    raise(SIGINT);
    std::this_thread::sleep_for(std::chrono::microseconds(500));
    ASSERT_FALSE(app->isRunning());
}

#endif