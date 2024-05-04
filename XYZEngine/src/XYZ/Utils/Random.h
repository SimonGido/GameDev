#pragma once
#include "XYZ/Core/Core.h"



namespace XYZ {
	XYZ_API uint32_t RandomNumber(uint32_t min, uint32_t max);
	XYZ_API int32_t  RandomNumber(int32_t min, int32_t max);
	XYZ_API float    RandomNumber(float min, float max);
	XYZ_API bool	 RandomBool(float chance = 0.5f);
}
