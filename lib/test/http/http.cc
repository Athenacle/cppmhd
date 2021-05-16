#include <chrono>

#include "http_app.h"

using namespace cppmhd;

TEST(Http, badRoute)
{
    App app("127.0.0.1", 65432);
    app.add<TestCtrl>(HttpMethod::GET, "////");

    auto ret = app.start();
    ASSERT_FALSE(app.isRunning());
    ASSERT_EQ(ret, CPPMHD_Error::CPPMHD_ROUTER_TREE_BUILD_FAILED);
}

TEST(Http, parseIpAddressFailed)
{
    App app("a.b.c.d", 65432);
    app.add<TestCtrl>(HttpMethod::GET, "/root");
    auto ret = app.start();

    ASSERT_FALSE(app.isRunning());
    ASSERT_EQ(ret, CPPMHD_Error::CPPMHD_LISTEN_ADDRESS_ERROR);
}

TEST(Http, listenFailed)
{
    App app("1.1.1.1", 0);
    app.add<TestCtrl>(HttpMethod::GET, "/root");
    auto ret = app.start();

    ASSERT_FALSE(app.isRunning());
    ASSERT_EQ(ret, CPPMHD_Error::CPPMHD_LISTEN_FAILED);
}
