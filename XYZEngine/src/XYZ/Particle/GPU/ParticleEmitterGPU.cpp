#include "stdafx.h"
#include "ParticleEmitterGPU.h"

#include "ParticleSystemGPU.h"

#include <glm/common.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtc/constants.hpp>

namespace XYZ {


	ParticleEmitterGPU::ParticleEmitterGPU(uint32_t stride)
		:
		EmissionRate(1000.0f),
		m_Stride(stride),
		m_Emitted(0.0f)
	{
	}


	uint32_t ParticleEmitterGPU::Emit(Timestep ts, std::byte* buffer, uint32_t bufferSize)
	{
		m_Emitted += EmissionRate * ts;
		uint32_t count = static_cast<uint32_t>(m_Emitted);

		// Make sure that we are in bounds of bufferSize
		count = std::min(count, bufferSize / m_Stride);

		//TODO: This can be multithreaded
		for (auto& mod : EmitterModules)
		{
			if (mod->Enabled)
			{
				mod->Generate(buffer, count);
			}
		}

		if (count > 0)
			m_Emitted = 0.0f;
		
		return count;
	}
	uint32_t ParticleEmitterGPU::Emit(Timestep ts)
	{
		m_Emitted += EmissionRate * ts;
		const uint32_t count = static_cast<uint32_t>(m_Emitted);
		
		if (count > 0)
			m_Emitted = 0.0f;

		return count;
	}
	void ParticleEmitterGPU::Generate(uint32_t count, std::byte* buffer, uint32_t bufferSize)
	{
		// Make sure that we are in bounds of bufferSize
		count = std::min(count, bufferSize / m_Stride);

		//TODO: This can be multithreaded
		for (auto& mod : EmitterModules)
		{
			if (mod->Enabled)
			{
				mod->Generate(buffer, count);
			}
		}
	}
}