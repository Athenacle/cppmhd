#include "test.h"


uint8_t *generateRandom(size_t size)
{
    auto ret = new uint8_t[size];
    auto count = size / sizeof(uint32_t);
    auto uint32p = reinterpret_cast<uint32_t *>(ret);
    auto end = ret + (count * sizeof(uint32_t));
    for (auto i = 0u; i < count; i++) {
        uint32p[i] = rand();
    }

    for (auto i = 0u; i < size % sizeof(uint32_t); i++) {
        end[i] = static_cast<uint8_t>(rand() & 0xff);
    }

    return ret;
}