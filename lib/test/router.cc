#include "router.h"

#include <gtest/gtest.h>

#include <random>

#include "format.h"
#include "logger.h"
#include "test.h"
#include "utils.h"

using namespace cppmhd;
using std::move;

void print(const std::vector<std::string>& i, std::string& out)
{
    out.clear();
    std::for_each(i.begin(), i.end(), [&out](const std::string& ii) { out = out.append("/").append(ii); });
}
size_t fact(uint8_t i)
{
    size_t ret = 1;
    for (uint8_t j = 1; j <= i; j++) {
        ret = ret * j;
    }
    return ret;
}

class TestHttpController : public HttpController
{
    std::string path_;
    static int i;

    int my;

    mutable const void* ptr;

  public:
    void setPtr(const void* p) const
    {
        this->ptr = p;
    }
    TestHttpController(const std::string& path) : path_(path)
    {
        my = i++;
        ptr = nullptr;
    }

    virtual void onRequest(HttpRequestPtr, HttpResponsePtr&)
    {
        if (ptr) {
            EXPECT_EQ(ptr, this);
        }
    }

    virtual ~TestHttpController() {}
};

int TestHttpController::i = 0;

void permutation(std::vector<std::string>& segment, std::vector<std::string>& out)
{
    const auto testCount = fact(segment.size());

    std::sort(segment.begin(), segment.end());

    out.reserve(testCount);

    std::string calc;

    do {
        print(segment, calc);
        out.emplace_back(calc);
    } while (std::next_permutation(segment.begin(), segment.end()));
}

TEST(Router, StaticRouterMatch)
{
    RouterBuilder rb;

    std::vector<std::string> paths = {"foo", "bar", "baz", "path", "fuzz"};
    std::vector<std::string> out;

    permutation(paths, out);

    for (const auto& p : out) {
        rb.add<TestHttpController>(HttpMethod::GET, p, p);
    }

    Router r(move(rb));
    ASSERT_TRUE(r.build());

    std::random_device rd;
    std::mt19937 g(rd());

    std::shared_ptr<HttpResponse> res;

    std::map<std::string, std::string> param;

    bool tsr;
    for (uint64_t i = 0; i < fact(paths.size()); i++) {
        std::string outs;
        std::shuffle(paths.begin(), paths.end(), g);
        print(paths, outs);

        auto req = std::make_shared<TestRequest>(HttpMethod::GET, outs);

        auto ctrl = r.forward(req.get(), param, tsr);
        ASSERT_NE(ctrl, nullptr);
        reinterpret_cast<const TestHttpController*>(ctrl)->setPtr(ctrl);
        ctrl->onRequest(req, res);
    }
}

TEST(Router, ParamMatch)
{
    RouterBuilder rb;

    std::vector<std::tuple<std::string, std::vector<std::tuple<std::string, bool, std::map<std::string, std::string>>>>>
        tests = {
            {"/foo/bar/prefix-{:id}",
             {{"/foo/bar/prefix-this-id", true, {{"id", "this-id"}}},
              {"/foo/bar/prefix-other-id", true, {{"id", "other-id"}}},
              {"/foo/bar/prefix-th/other", false, {{}}},
              {"/foo/bar/prefix-th/", false, {{}}}}},

            {"/foo/bar/other-prefix-{:id}",
             {{"/foo/bar/other-prefix-this-id", true, {{"id", "this-id"}}},
              {"/foo/bar/other-prefix-other-id", true, {{"id", "other-id"}}},
              {"/foo/bar/other-prefix-th/other", false, {{}}},
              {"/foo/bar/other-prefix-th/", false, {{}}}}},

            {"/bar/foo/some-{\\d+:number}",
             {{"/bar/foo/some-123", true, {{"number", "123"}}},
              {"/bar/foo/some-1", true, {{"number", "1"}}},
              {"/bar/foo/some-abc", false, {{}}},
              {"/bar/foo/some-", false, {{}}}}},

            {"/bar/foo/next/{\\d+:number}",
             {{"/bar/foo/next/123", true, {{"number", "123"}}},
              {"/bar/foo/next/1", true, {{"number", "1"}}},
              {"/bar/foo/next/abc", false, {{}}},
              {"/bar/foo/next/", false, {{}}}}},

            {"/baz/bar/{\\d+:id}/",
             {{"/baz/bar/aaa/", false, {{}}}, {"/baz/bar/123", false, {{}}}, {"/baz/bar/123/", true, {{"id", "123"}}}}},

            {"/bar/foo/other/{:name}",
             {{"/bar/foo/other", false, {{}}},
              {"/bar/foo/other/", true, {{"name", ""}}},
              {"/bar/foo/other//", true, {{"name", ""}}},
              {"/bar/foo/other/apple", true, {{"name", "apple"}}}}},

            {"/bar/foo/some/{:name}/{\\d+:id}/{\\w+:pwd}",
             {{"/bar/foo/some", false, {{}}},
              {"/bar/foo/some/", false, {{}}},
              {"/bar/foo/some/apple/bad/aa", false, {{}}},
              {"/bar/foo/some/apple/123/aa", true, {{"name", "apple"}, {"id", "123"}, {"pwd", "aa"}}},
              {"/bar/foo/some/apple", false, {{}}}}},

            {"/root/{**:name}",
             {{"/root/aaaaa", true, {{"name", "aaaaa"}}},
              {"/root/bbbbb", true, {{"name", "bbbbb"}}},
              {"/root/", true, {{"name", ""}}},
              {"/root/a/b/c/d", true, {{"name", "a/b/c/d"}}}}},

            {"/all/other{**}", {{"/all/othera", true, {{"1", "a"}}}}},
            {"/all/1-other{a\\d+}", {{"/all/1-othera12", true, {{"1", "a12"}}}}},

            {"/p/w/no/{ab?c+}/{\\w+}/padding/{b[cd]ef?}/{\\d+}/{}",
             {{"/p/w/no/ac/apple/padding/bce/123/banana",
               true,
               {{"1", "ac"}, {"2", "apple"}, {"3", "bce"}, {"4", "123"}, {"5", "banana"}}},
              {"/p/w/no/error/apple/padding/bce/123/banana", false, {{}}}}},

            {"/all/all{**:name}",
             {{"/all/aaaaa", false, {{"name", "aaaaa"}}},
              {"/all/bbbbb", false, {{"name", "bbbbb"}}},
              {"/all/", false, {{}}},
              {"/all/a/b/c/d", false, {{"name", "a/b/c/d"}}},
              {"/all/all", true, {{"name", ""}}},
              {"/all/allabc", true, {{"name", "abc"}}},
              {"/all/alla/b/c/d", true, {{"name", "a/b/c/d"}}}}}};

    for (const auto& t : tests) {
        const auto& path = std::get<0>(t);
        rb.add<TestHttpController>(HttpMethod::GET, path.c_str(), std::get<0>(t));
    }

    Router r(move(rb));
    ASSERT_TRUE(r.build());

    std::shared_ptr<HttpResponse> resp;

    std::map<std::string, std::string> params;

    for (const auto& t : tests) {
        bool tsr;
        for (auto& c : std::get<1>(t)) {
            params.clear();

            auto req = std::make_shared<TestRequest>(HttpMethod::GET, std::get<0>(c));
            auto ctrl = r.forward(req.get(), params, tsr);

            if (std::get<1>(c)) {
                EXPECT_NE(ctrl, nullptr) << std::get<0>(t) << "\t" << std::get<0>(c);
                EXPECT_EQ(params, std::get<2>(c));

                if (ctrl) {
                    reinterpret_cast<const TestHttpController*>(ctrl)->setPtr(ctrl);
                    ctrl->onRequest(req, resp);
                }
            } else {
                EXPECT_EQ(ctrl, nullptr) << std::get<0>(t) << "\t" << std::get<0>(c);
            }
        }
    }
}

TEST(Router, special)
{
    RouterBuilder rb;

    rb.add<TestHttpController>(HttpMethod::GET, "/bar/foo/{**:all}", "");
    rb.add<TestHttpController>(HttpMethod::GET, "/bar/foo/", "");

    Router r(move(rb));
    ASSERT_TRUE(r.build());
}

TEST(Router, TSR)
{
    RouterBuilder rb;

    std::vector<std::tuple<std::string, std::string>> pairs = {

        {"/root/abc/", "/root/abc"},
        // {"/root/foo", "/root/"},
        {"/root/first/{:name}/", "/root/first/abc"},
        {"/root/some/{**}", "/root/some"}

    };

    for (const auto& p : pairs) {
        rb.add<TestHttpController>(HttpMethod::GET, std::get<0>(p), std::get<1>(p));
    }

    HttpResponsePtr resp;
    Router r(move(rb));

    ASSERT_TRUE(r.build());

    std::map<std::string, std::string> param;
    for (const auto& p : pairs) {
        auto req = std::make_shared<TestRequest>(HttpMethod::GET, std::get<1>(p));
        bool tsr;
        auto ctrl = r.forward(req.get(), param, tsr);
        EXPECT_EQ(ctrl, nullptr);
        EXPECT_TRUE(tsr) << FORMAT("Route: '{}', Request: '{}'", std::get<0>(p), std::get<1>(p));
    }
}

TEST(Router, combined)
{
    std::random_device rd;

    auto genMethod = [&rd]() {
        std::vector<HttpMethod> methods = {HttpMethod::GET,
                                           HttpMethod::HEAD,
                                           HttpMethod::POST,
                                           HttpMethod::PUT,
                                           HttpMethod::DELETE,
                                           HttpMethod::OPTIONS,
                                           HttpMethod::CONNECT,
                                           HttpMethod::TRACE,
                                           HttpMethod::PATCH};
        return methods[rd() % methods.size()];
    };

    RouterBuilder prefix1;
    std::string prefix = "/static";

    std::vector<std::tuple<HttpMethod, std::string>> cases;

    std::vector<std::string> segments = {"foo", "bar", "baz", "path", "fuzz"};
    std::vector<std::string> prefixRoutes;
    permutation(segments, prefixRoutes);

    for (int i = 0; i < 4; i++) {
        for (const auto& pr : prefixRoutes) {
            auto whole = prefix + pr;
            auto mtd = genMethod();

            auto find =
                std::find_if(cases.begin(), cases.end(), [&whole, mtd](const std::tuple<HttpMethod, std::string>& p) {
                    return std::get<0>(p) == mtd && std::get<1>(p) == whole;
                });

            if (find == cases.end()) {
                prefix1.add<TestHttpController>(mtd, pr, prefix + pr);
                cases.emplace_back(mtd, whole);
            }
        }
    }

    RouterBuilder rb;
    rb.add(prefix, (prefix1));

    Router r(move(rb));
    ASSERT_TRUE(r.build());

    std::map<std::string, std::string> param;
    bool tsr;
    HttpResponsePtr resp;
    for (const auto& p : cases) {
        auto req = std::make_shared<TestRequest>(std::get<0>(p), std::get<1>(p));
        auto ctrl = r.forward(req.get(), param, tsr);
        ASSERT_TRUE(ctrl);
        reinterpret_cast<const TestHttpController*>(ctrl)->setPtr(ctrl);
        ctrl->onRequest(req, resp);
    }
}