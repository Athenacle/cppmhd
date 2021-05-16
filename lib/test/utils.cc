#include "utils.h"

#include <gtest/gtest.h>

#define FORMAT_INETADDRESS
#include "format.h"

using namespace cppmhd;

TEST(utils, split)
{
    std::vector<std::string> ins = {
        ("/a/b/c/d/e/f"),
        ("//a/b/c/d/e/f"),
        ("/a/b/c/d/e/f/"),
        ("/a/b/c/d/e/f//"),
        ("/a/b//c///d/e/f/"),
    };

    std::vector<std::string> result;

    auto fn = [](const std::vector<std::string>& out) {
        ASSERT_EQ(out.size(), 6);
        char cs[] = {'a', 'b', 'c', 'd', 'e', 'f'};

        for (size_t i = 0; i < sizeof(cs); i++) {
            ASSERT_EQ(out[i], std::string(1, cs[i]));
        }
    };

    for (const auto& s : ins) {
        result.clear();
        cppmhd::split(s, result, "/");

        fn(result);
    }
}


TEST(utils, badDns)
{
    std::vector<InetAddress> out;
    std::string query = "some-bad-bad-bad-bad-bad-domain";

    EXPECT_FALSE(InetAddress::fromHost(out, query, 80));
}


TEST(utils, dns)
{
    std::vector<InetAddress> out;
    std::string query;
#ifdef PROJECT_LOCAL_DOMAIN
    query = PROJECT_LOCAL_DOMAIN;
#endif

    if (query == LOCALHOST || query.length() == 0) {
        query = "www.baidu.com";
    }

    EXPECT_TRUE(InetAddress::fromHost(out, query, 80));
}

TEST(utils, IPv4)
{
    std::string v4 = "1.2.3.4";

    InetAddress address;
    ASSERT_TRUE(InetAddress::parse(address, v4, 60));
    ASSERT_FALSE(address.isV6());
    ASSERT_EQ(address.port(), 60u);

    auto f = FORMAT("{:a}", address);

    EXPECT_EQ(f, "1.2.3.4:60") << f;

    InetAddress wrong;
    v4 = "a.b.c.d";
    ASSERT_FALSE(InetAddress::parse(wrong, v4, 60));
}

TEST(utils, IPv6)
{
    std::string v6 = "fe80::a400:8182:e2c4:575a";
    InetAddress address;
    ASSERT_TRUE(InetAddress::parse(address, v6, 60));
    ASSERT_TRUE(address.isV6());
    ASSERT_EQ(address.port(), 60u);

    auto f = FORMAT("{:a}", address);
    EXPECT_EQ(f, FORMAT("{}:{}", v6, 60u)) << f;

    InetAddress wrong;
    v6 = "fe80::a400:8182:e2c4:575a:fe80::a400:8182:e2c4:575a";
    ASSERT_FALSE(InetAddress::parse(wrong, v6, 60));
}

TEST(utils, Regex)
{
    Regex re;
    ASSERT_TRUE(Regex::compileRegex(re, "^ab?c{1,2}\\d+$"));

    const char str1[] = "abc1";

    ASSERT_TRUE(re.match(str1));
    ASSERT_TRUE(re.match(std::string(str1)));
    ASSERT_TRUE(re.match(str1, str1 + sizeof(str1) - 1));

    // re-compile. test whether there exist memory leak
    ASSERT_TRUE(Regex::compileRegex(re, "^ab?c{1,2}\\d+$"));

    Regex re2;
    ASSERT_TRUE(Regex::compileRegex(re2, "^ab?c{1,2}\\d+$"));
    ASSERT_EQ(re, re2);

    Regex empty;
    ASSERT_FALSE(empty.match(str1));
    ASSERT_FALSE(empty.match(std::string(str1)));
    ASSERT_FALSE(empty.match(str1, str1 + sizeof(str1) - 1));

    Regex failed;
    ASSERT_FALSE(Regex::compileRegex(failed, "**"));
}

TEST(utils, RegexCapture)
{
    Regex re;
    ASSERT_TRUE(Regex::compileRegex(re,
                                    "\\[(?<hours>\\d+):(?<minutes>\\d+):(?<seconds>\\d+)(\\.(?<"
                                    "milliseconds>\\d+))?\\] \\<(?<nickname>\\w+)\\> (?<message>.*)"));

    const char str1[] = "[12:34:56] <SomeName> This is a Long Long Long Message";

    std::map<std::string, std::string> result;

    ASSERT_TRUE(re.match(str1, result));
    ASSERT_EQ(result.size(), 6u);
    ASSERT_EQ(result["hours"], "12");
    ASSERT_EQ(result["minutes"], "34");
    ASSERT_EQ(result["seconds"], "56");
    ASSERT_EQ(result["milliseconds"], "");
    ASSERT_EQ(result["nickname"], "SomeName");
    ASSERT_EQ(result["message"], "This is a Long Long Long Message");

    result.clear();
    Regex emptyGroup;
    ASSERT_TRUE(Regex::compileRegex(emptyGroup, "^a+b*c$"));
    ASSERT_TRUE(emptyGroup.match("aac", result));
    ASSERT_EQ(result.size(), 0);
}