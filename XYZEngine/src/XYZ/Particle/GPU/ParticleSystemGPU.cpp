#include "stdafx.h"
#include "ParticleSystemGPU.h"

#include <glm/common.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtc/constants.hpp>

namespace XYZ {
	

	ParticleSystemGPU::ParticleSystemGPU(ParticleSystemLayout inputLayout, ParticleSystemLayout outputLayout, uint32_t maxParticles)
		:
		m_InputLayout(std::move(inputLayout)),
		m_OutputLayout(std::move(outputLayout)),
		m_EmittedParticles(0),
		m_EmissionCount(0)
	{
		m_ParticleBuffer.setMaxParticles(maxParticles, m_InputLayout.GetStride());
	}


	void ParticleSystemGPU::Reset()
	{
		m_EmittedParticles = 0;
		m_EmissionCount = 0;
	}

	const EmissionResult ParticleSystemGPU::LastEmission() const
	{
		const uint32_t offsetParticles = m_EmittedParticles - m_EmissionCount;
		const uint32_t dataOffset = offsetParticles * m_ParticleBuffer.GetStride();
		const uint32_t size = m_EmissionCount * m_ParticleBuffer.GetStride();

		EmissionResult result(m_ParticleBuffer.GetData(offsetParticles), size, dataOffset);
		m_EmissionCount = 0;
		return result;
	}

	ParticleView ParticleSystemGPU::Emit(uint32_t particleCount)
	{
		particleCount = std::min(particleCount, m_ParticleBuffer.GetMaxParticles() - m_EmittedParticles);
		
		const uint32_t bufferSize = particleCount * GetInputStride();
		std::byte* particleBuffer = m_ParticleBuffer.GetData(m_EmittedParticles);

		// Make sure we do not emit more particles than available
		m_EmittedParticles += particleCount;
		m_EmissionCount += particleCount;
		return ParticleView{ particleBuffer, bufferSize };
	}
	
	ParticleBuffer::ParticleBuffer(uint32_t maxParticles, uint32_t stride)
		:
		m_Stride(stride)
	{
		m_Data.resize(maxParticles * stride);
		memset(m_Data.data(), 0, m_Data.size());
	}


	std::byte* ParticleBuffer::GetData(uint32_t particleOffset)
	{
		uint32_t offset = particleOffset * m_Stride;
		return &m_Data.data()[offset];
	}
	const std::byte* ParticleBuffer::GetData(uint32_t particleOffset) const
	{
		uint32_t offset = particleOffset * m_Stride;
		return &m_Data.data()[offset];
	}


	void ParticleBuffer::setMaxParticles(uint32_t maxParticles, uint32_t stride)
	{
		m_Stride = stride;
		m_Data.resize(maxParticles * stride);
		memset(m_Data.data(), 0, m_Data.size());
	}

	ParticleSystemGPUShaderGenerator::ParticleSystemGPUShaderGenerator(const Ref<ParticleSystemGPU>& particleSystem)
	{
		auto& outputLayout = particleSystem->GetOutputLayout();
		auto& inputLayout = particleSystem->GetInputLayout();

		auto& outputVariables = outputLayout.GetVariables();
		auto& inputVariables = inputLayout.GetVariables();

		m_SourceCode += "//#type compute\n";
		m_SourceCode += "#version 460\n";
		m_SourceCode += "#include \"Resources/Shaders/Includes/Math.glsl\"\n";

		addStruct("DrawCommand", {
			Variable{"uint", "Count", sizeof(uint32_t) },
			Variable{"uint", "InstanceCount", sizeof(uint32_t) },
			Variable{"uint", "FirstIndex", sizeof(uint32_t) },
			Variable{"uint", "BaseVertex", sizeof(uint32_t) },
			Variable{"uint", "BaseInstance", sizeof(uint32_t) }
		});


		addStruct(outputLayout.GetName(), particleVariablesToVariables(outputVariables));
		addStruct(inputLayout.GetName(), particleVariablesToVariables(inputVariables));

		addUniforms("Uniform", "u_Uniforms", {
			Variable{"uint",  "CommandCount"},
			Variable{"float", "Timestep"},
			Variable{"float", "Speed"},
			Variable{"uint",  "EmittedParticles"},
			Variable{"bool",  "Loop"},
		});

		addSSBO(5, "DrawCommand", { {"DrawCommand", "Command", 0, true} });
		addSSBO(6, outputLayout.GetName(), { {outputLayout.GetName(), "Outputs", 0, true} });
		addSSBO(7, inputLayout.GetName(), { {inputLayout.GetName(), "Inputs", 0, true} });

		addEntryPoint(32, 32, 1);
	}

	void ParticleSystemGPUShaderGenerator::addStruct(const std::string_view name, const std::vector<Variable>& variables)
	{
		uint32_t offset = 0;
		if (!variables.empty())
		{
			m_SourceCode += fmt::format("struct {}", name) + "\n{\n";
			for (auto& var : variables)
			{
				if (var.IsArray)
				{
					m_SourceCode += fmt::format("	{} {}[];\n", var.Type, var.Name);
				}
				else
				{
					m_SourceCode += fmt::format("	{} {};\n", var.Type, var.Name);
				}
				offset += var.Size;
			}

			uint32_t paddingSize = Math::RoundUp(offset, 16) - offset;
			if (paddingSize != 0)
			{
				m_SourceCode += fmt::format("	uint Padding[{}];\n", paddingSize / sizeof(uint32_t));
			}
			m_SourceCode += "};\n";
		}
	}

	void ParticleSystemGPUShaderGenerator::addSSBO(uint32_t binding, const std::string_view name, const std::vector<Variable>& variables)
	{
		m_SourceCode += fmt::format("layout(std430, binding = {}) buffer buffer_{}\n", binding, name);
		m_SourceCode += "{\n";
		for (const auto& var : variables)
		{
			if (var.IsArray)
			{
				m_SourceCode += fmt::format("	{} {}[];\n", var.Type, var.Name);
			}
			else
			{
				m_SourceCode += fmt::format("	{} {};\n", var.Type, var.Name);
			}
		}
		m_SourceCode += "\n};\n";
	}

	void ParticleSystemGPUShaderGenerator::addUniforms(const std::string_view name, const std::string_view declName, const std::vector<Variable>& variables)
	{
		m_SourceCode += fmt::format("layout(push_constant) uniform {}\n", name);
		m_SourceCode += "{\n";
		for (const auto& var : variables)
		{
			if (var.IsArray)
			{
				m_SourceCode += fmt::format("	{} {}[];\n", var.Type, var.Name);
			}
			else
			{
				m_SourceCode += fmt::format("	{} {};\n", var.Type, var.Name);
			}
		}
		m_SourceCode += "}";
		m_SourceCode += fmt::format("{};\n", declName);
	}

	void ParticleSystemGPUShaderGenerator::addEntryPoint(uint32_t groupX, uint32_t groupY, uint32_t groupZ)
	{
		m_SourceCode += fmt::format("layout(local_size_x = {}, local_size_y = {}, local_size_z = {}) in;\n", groupX, groupY, groupZ);
		m_SourceCode +=
			"void main(void)\n"
			"{\n"
			"	uint id = gl_GlobalInvocationID.x + gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x;\n"																													   \
			"	if (id >= u_Uniforms.EmittedParticles)\n"
			"		return;\n"
			"	Update(id);\n"
			"}";
	}
	void ParticleSystemGPUShaderGenerator::addUpdate()
	{

	}

	std::vector<ParticleSystemGPUShaderGenerator::Variable> ParticleSystemGPUShaderGenerator::particleVariablesToVariables(const std::vector<ParticleVariable>& variables)
	{
		std::vector<Variable> tempVariables;
		tempVariables.reserve(variables.size());
		//for (auto& var : variables)
		//	tempVariables.push_back(Variable{ VariableTypeToGLSL(var.Type), var.Name, var.Size, var.IsArray});

		return tempVariables;
	}
	EmissionResult::EmissionResult(const std::byte* data, uint32_t size, uint32_t offset)
		:
		EmittedData(data),
		EmittedDataSize(size),
		DataOffset(offset)
	{
	}
	ParticleView::ParticleView(std::byte* data, uint32_t size)
		:
		m_Data(data),
		m_Size(size)
	{
	}
}