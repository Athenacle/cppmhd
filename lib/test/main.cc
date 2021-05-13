#include "config.h"

#include <gmock/gmock.h>

#ifdef TEST_ENABLE_CURL
#include <curl/curl.h>
#endif

int main(int argc, char* argv[])
{
#ifdef TEST_ENABLE_CURL
    curl_global_init(CURL_GLOBAL_ALL);
#endif

    srand(time(nullptr));
    testing::InitGoogleMock(&argc, argv);
    auto ret = RUN_ALL_TESTS();
#ifdef TEST_ENABLE_CURL
    curl_global_cleanup();
#endif
    return ret;
}