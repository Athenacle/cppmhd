#include "http_app.h"

#include <fstream>

#include "logger.h"

ShaCalc::ShaCalc()
{
    finished = false;
#ifdef TEST_ENABLE_OPENSSL
    SHA256_Init(&ctx);
    memset(hash, 0, sizeof(hash));
#else
    start = 0xdeefabcd01020304;
#endif
}
void ShaCalc::update(const void* in, size_t size)
{
#ifdef TEST_ENABLE_OPENSSL
    SHA256_Update(&ctx, in, size);
#else
    auto uint8p = reinterpret_cast<const uint8_t*>(in);
    for (auto i = 0u; i < size; i++) {
        auto c = uint8p[i];
        start = ((start << 8) & (~0xabcdef0123)) | c;
    }
#endif
}

void ShaCalc::final()
{
    finished = true;
#ifdef TEST_ENABLE_OPENSSL
    SHA256_Final(hash, &ctx);
#endif
}

bool ShaCalc::operator==(const ShaCalc& other) const
{
#ifdef TEST_ENABLE_OPENSSL
    return memcmp(hash, other.hash, sizeof(hash)) == 0;
#else
    return start == other.start;
#endif
}

std::string ShaCalc::format() const
{
#ifdef TEST_ENABLE_OPENSSL
    char buffer[65];
    for (auto i = 0u; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(buffer + (i * 2), "%02x", hash[i]);
    }
    return FORMAT("{}", buffer);
#else
    return fmt::to_string(start);
#endif
}

bool calcFile(ShaCalc& calc, const std::string fullName)
{
    calc = ShaCalc();
    std::ifstream file(fullName);
    if (file.fail()) {
        ERROR("open file '{}' failed: '{}'.", fullName, strerror((errno)));
        return false;
    }
    const size_t bufsize = 8 * 1024;
    auto buffer = new char[bufsize];
    size_t i = 0;
    do {
        file.read(buffer, bufsize);
        auto s = file.gcount();
        if (s > 0) {
            calc.update(buffer, s);
            i += s;
            if (file.good()) {
                continue;
            }
        } else {
            break;
        }
    } while (true);

    auto ret = true;
    if (file.eof()) {
        calc.final();
        INFO("hash file '{}' success: '{}', fileSize: {}", fullName, calc.format(), i);
    } else {
        ret = false;
    }
    delete[] buffer;
    file.close();
    return ret;
}

/////// HttpApp
uint8_t HttpApp::lv = 0;
const std::string HttpApp::host = "127.0.0.1";
const uint16_t HttpApp::port = 65432;

void HttpApp::run()
{
    app->start([this]() { this->startUp.wait(); });
}

void HttpApp::start()
{
    thr = std::thread(std::bind(&HttpApp::run, this));
    startUp.wait();
}

void HttpApp::SetUp()
{
    app = new App(host, port);
    app->threadCount(2);
    myName = FORMAT("/{}", UnitTest::GetInstance()->current_test_info()->name());
    EXPECT_EQ(app->threadCount(), 2);
}


void HttpApp::TearDown()
{
    if (app) {
        if (app->isRunning()) {
            app->stop();
        }
        if (thr.joinable()) {
            thr.join();
        }
        delete app;
        app = nullptr;
    }
}