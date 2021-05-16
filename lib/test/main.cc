#include "config.h"

#include <gmock/gmock.h>

#include "logger.h"
#include "test.h"

#ifdef TEST_ENABLE_CURL
#include <curl/curl.h>
#endif

int main(int argc, char* argv[])
{
#ifdef TEST_ENABLE_CURL
    auto flag = CURL_GLOBAL_ALL;
#ifdef CURL_GLOBAL_ACK_EINTR
    flag |= CURL_GLOBAL_ACK_EINTR;
#endif
#ifdef ON_WINDOWS
    flag |= CURL_GLOBAL_WIN32;
#endif
    curl_global_init(flag);
#endif

#ifdef RUN_ID
    LOG_INFO("start unittest run: {}", STRINGIFY(RUN_ID));
#endif

    srand(time(nullptr));
    testing::InitGoogleMock(&argc, argv);
#ifdef ON_WINDOWS
    testing::InitGoogleTest(&argc, argv);
#endif
    auto ret = RUN_ALL_TESTS();
#ifdef TEST_ENABLE_CURL
    curl_global_cleanup();
#endif
    return ret;
}