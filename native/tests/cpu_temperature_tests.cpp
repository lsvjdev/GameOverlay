#include "../cpu_temperature_math.h"
#include <cassert>
#include <cstdint>

int main()
{
    assert(DecodeAmdTemperature(0) == -1);
    assert(DecodeAmdTemperature(0xffffffffu) == -1);
    assert(DecodeAmdTemperature((80u * 8u) << 21) == 80);
    assert(DecodeAmdTemperature(((90u + 49u) * 8u << 21) | 0x80000u) == 90);
    assert(DecodeAmdTemperature(((80u + 49u) * 8u << 21) | 0x30000u) == 80);
    assert(DecodeAmdTemperature((127u * 8u) << 21) == -1);
}
