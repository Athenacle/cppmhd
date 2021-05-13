// args: -only_ascii=1
// args: -detect_leaks=1
// args: -print_pcs=1
// args: -print_final_stats=1
// args: -print_corpus_stats=1

#include <cstddef>
#include <cstdint>

#include "fuzz.h"
#include "router.h"

using namespace cppmhd;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
    RouterBuilder rb;

    std::string s(reinterpret_cast<const char*>(Data), Size);
    rb.add<Ctrl>(HttpMethod::GET, s.c_str());
    Router b(std::move(rb));
    b.build();

    return 0;
}
