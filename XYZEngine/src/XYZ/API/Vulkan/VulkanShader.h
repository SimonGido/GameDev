#pragma once
#include "XYZ/Renderer/Shader.h"

#include "XYZ/Renderer/Material.h"

#include "Vulkan.h"
#include "VulkanMemoryAllocator/vk_mem_alloc.h"


#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>

namespace XYZ {

	class VulkanShader : public Shader
	{
	public:
		struct UniformBuffer
		{
			uint32_t Size = 0;
			uint32_t BindingPoint = 0;
			std::string Name;
			VkShaderStageFlagBits ShaderStage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
		};

		struct StorageBuffer
		{
			VmaAllocation MemoryAlloc = nullptr;
			uint32_t Size = 0;
			uint32_t BindingPoint = 0;
			std::string Name;
			VkShaderStageFlagBits ShaderStage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
		};

		struct ImageSampler
		{
			uint32_t BindingPoint = 0;
			uint32_t DescriptorSet = 0;
			uint32_t ArraySize = 0;
			std::string Name;
			VkShaderStageFlagBits ShaderStage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
		};
		struct PushConstantRange
		{
			VkShaderStageFlagBits ShaderStage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
			uint32_t Offset = 0;
			uint32_t Size = 0;
		};

		struct ShaderDescriptorSet
		{
			std::unordered_map<uint32_t, UniformBuffer*> UniformBuffers;
			std::unordered_map<uint32_t, StorageBuffer*> StorageBuffers;
			std::unordered_map<uint32_t, ImageSampler>   ImageSamplers;
			std::unordered_map<uint32_t, ImageSampler>   StorageImages;

			std::unordered_map<std::string, VkWriteDescriptorSet> WriteDescriptorSets;

			operator bool() const { return !(StorageBuffers.empty() && UniformBuffers.empty() && ImageSamplers.empty() && StorageImages.empty()); }
		};

		struct DescriptorSet
		{
			VkDescriptorSetLayout DescriptorSetLayout;
			ShaderDescriptorSet	  ShaderDescriptorSet;
		};

	public:
		VulkanShader(const std::string& path);
		VulkanShader(const std::string& name, const std::string& path);
		virtual ~VulkanShader() override;

		virtual void Reload(bool forceCompile = false) override;

		inline virtual const std::string&		GetPath() const override { return m_AssetPath; };
		inline virtual const std::string&		GetName() const override { return m_Name; }
		virtual size_t							GetHash() const override;
		virtual bool							IsCompiled() const override;
		virtual const std::unordered_map<std::string, ShaderBuffer>& GetBuffers() const override { return m_Buffers; }
		virtual const std::unordered_map<std::string, ShaderResourceDeclaration>& GetResources() const override { return m_Resources; }

		const std::vector<DescriptorSet>&				   GetDescriptorSets()		 const { return m_DescriptorSets; }
		const std::vector<PushConstantRange>&			   GetPushConstantRanges()	 const { return m_PushConstantRanges; }
		const std::vector<VkPipelineShaderStageCreateInfo> GetPipelineShaderStageCreateInfos() const { return m_PipelineShaderStageCreateInfos; }
		const VkWriteDescriptorSet*						   GetDescriptorSet(const std::string& name, uint32_t set) const;
		std::pair<const VkWriteDescriptorSet*, uint32_t>   GetDescriptorSet(const std::string& name) const;

		std::vector<VkDescriptorSetLayout>				   GetAllDescriptorSetLayouts() const;
		size_t											   GetVertexBufferSize()		const { return m_VertexBufferSize; }
		
	private:
		void compileOrGetVulkanBinaries(std::unordered_map<VkShaderStageFlagBits, std::vector<uint32_t>>& outputBinary, bool forceCompile);
		void createProgram(const std::unordered_map<VkShaderStageFlagBits, std::vector<uint32_t>>& shaderData);
		void preProcess(const std::string& source);
		
		void reflectAllStages(const std::unordered_map<VkShaderStageFlagBits, std::vector<uint32_t>>& shaderData);
		void reflectStage(VkShaderStageFlagBits stage, const std::vector<uint32_t>& shaderData);
		void reflectConstantBuffers(const spirv_cross::Compiler& compiler, VkShaderStageFlagBits stage, spirv_cross::SmallVector<spirv_cross::Resource>& buffers);
		void reflectStorageBuffers(const spirv_cross::Compiler& compiler, VkShaderStageFlagBits stage, spirv_cross::SmallVector<spirv_cross::Resource>& buffers);
		void reflectUniformBuffers(const spirv_cross::Compiler& compiler, VkShaderStageFlagBits stage, spirv_cross::SmallVector<spirv_cross::Resource>& buffers);
		void reflectSampledImages(const spirv_cross::Compiler& compiler, VkShaderStageFlagBits stage, spirv_cross::SmallVector<spirv_cross::Resource>& sampledImages);
		void reflectStorageImages(const spirv_cross::Compiler& compiler, VkShaderStageFlagBits stage, spirv_cross::SmallVector<spirv_cross::Resource>& storageImages);



		void createDescriptorSetLayout();
		void destroy();	

		size_t getBuffersSize() const;
	private:
		bool					   m_Compiled;
		std::string				   m_Name;
		std::string				   m_AssetPath;
		std::vector<DescriptorSet> m_DescriptorSets;

		std::unordered_map<VkShaderStageFlagBits, std::string>		m_ShaderSources;
		std::vector<VkPipelineShaderStageCreateInfo>				m_PipelineShaderStageCreateInfos;

		std::unordered_map<std::string, ShaderResourceDeclaration>	m_Resources;
		std::vector<PushConstantRange>								m_PushConstantRanges;
		
		std::unordered_map<std::string, ShaderBuffer>				m_Buffers;
		size_t														m_VertexBufferSize;
	};
}