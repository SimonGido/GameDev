#include "stdafx.h"
#include "Random.h"


namespace XYZ {

    static thread_local std::random_device s_Device;
    static thread_local std::mt19937       s_RandEng(s_Device());
 

    uint32_t RandomNumber(uint32_t min, uint32_t max)
    {
        std::uniform_int_distribution<uint32_t> dist(min, max);
        return dist(s_RandEng);
    }

    XYZ_API int32_t RandomNumber(int32_t min, int32_t max)
    {
        std::uniform_int_distribution<int32_t> dist(min, max);
        return dist(s_RandEng);
    }

    float RandomNumber(float min, float max)
    {
        std::uniform_real_distribution<float> dist(min, max);
        return dist(s_RandEng);
    }

    XYZ_API bool RandomBool(float chance)
    {
        float rand = RandomNumber(0.0f, 1.0f);
        return rand < chance;
    }

}