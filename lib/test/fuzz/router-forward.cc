// args: -only_ascii=1
// args: -detect_leaks=1
// args: -print_pcs=1
// args: -print_final_stats=1
// args: -print_corpus_stats=1
// args: -dict=FZ_ROOT/router-forward.dict

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "../test.h"
#include "fuzz.h"
#include "router.h"

using namespace cppmhd;

void print(const std::vector<std::string>& i, std::string& out)
{
    out.clear();
    std::for_each(i.begin(), i.end(), [&out](const std::string& i) { out = out.append("/").append(i); });
}
inline uint64_t fact(uint8_t i)
{
    uint64_t ret = 1;
    for (uint8_t j = 1; j <= i; j++) {
        ret = ret * j;
    }
    return ret;
}

Router* r = nullptr;
void destroy()
{
    delete r;
}

Router* router()
{
    RouterBuilder rb;

    std::vector<std::string> paths = {"foo", "bar", "baz", "path", "fuzz", "zoo", "banana"};

    std::vector<std::string> param = {"/a/b/c/d/e/f/g/{\\d+:id}",
                                      "/b/c/d/e/f/g/h/{:name}/{:type}",
                                      "/c/d/e/f/g/h/{f+:ffs}/{ab?c+:abc}",
                                      "/e/g/h/c/h/e/h/i/j/{ccdb}/k",
                                      "/1/2/3/4/{}",
                                      "/5/6/7/8/{id}",
                                      "/8/9/7/10/11/{a+:b}"};

    const auto testCount = fact(paths.size());

    std::sort(paths.begin(), paths.end());

    std::string outs;
    outs.reserve(40);
    do {
        print(paths, outs);
        rb.add<Ctrl>(HttpMethod::GET, outs.c_str());
    } while (std::next_permutation(paths.begin(), paths.end()));

    for (const auto& p : param) {
        rb.add<Ctrl>(HttpMethod::GET, p.c_str());
    }
    auto ret = new Router(std::move(rb));
    assert(ret->build());

    atexit(destroy);

    r = ret;
    return ret;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
    static auto r = router();
    bool tsr;
    std::string url(reinterpret_cast<const char*>(Data), Size);
    TestRequest req(HttpMethod::GET, url);
    std::map<std::string, std::string> param;

    r->forward(&req, param, tsr);

    return 0;
}