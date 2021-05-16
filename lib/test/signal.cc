#include "config.h"

#ifdef ON_UNIX
#include <cppmhd/app.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "test.h"

using namespace testing;
using namespace cppmhd;


using namespace std::placeholders;

TEST(utils, SignalInstaller)
{
    SHMock mock;
    App::setSignalHandler(SIGUSR1, std::bind(&SHMock::handler, &mock, _1, _2));
    EXPECT_CALL(mock, handler(_, _)).Times(0);
}


#endif
