#pragma once
#include "XYZ/Renderer/Shader.h"

#include "XYZ/Renderer/Material.h"
#include "XYZ/Renderer/ShaderParser.h"

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
			std::unordered_map<uint32_t, UniformBuffer>  UniformBuffers;
			std::unordered_map<uint32_t, StorageBuffer>  StorageBuffers;
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


		struct SpecializationInfo
		{
			VkSpecializationInfo Info;
			std::vector<VkSpecializationMapEntry> MapEntries;
		};

		

		template <typename T>
		using StageMap = std::unordered_map<VkShaderStageFlagBits, T>;

	public:
		VulkanShader(const std::string& path, size_t sourceHash, bool forceCompile);
		VulkanShader(const std::string& name, const std::string& path, size_t sourceHash, bool forceCompile);
		VulkanShader(const std::string& name, const std::string& vertexPath, const std::string& fragmentPath, size_t sourceHash, bool forceCompile);

		virtual ~VulkanShader() override;

		virtual void Reload(bool forceCompile = false) override;
	
		virtual const std::string&				 GetPath() const override { return m_FilePath; };
		virtual const std::string&				 GetName() const override { return m_Name; }
		virtual const std::string&				 GetSource() const override { return m_Source; };
		virtual const std::vector<BufferLayout>& GetLayouts() const override { return m_Layouts; }
		virtual size_t							 GetHash() const override;
		virtual size_t							 GetVertexBufferSize() const override{ return m_VertexBufferSize; }
		virtual const std::vector<uint32_t>&	 GetShaderData(ShaderType type) const override;
		virtual bool							 IsCompute()  const override;
		virtual bool							 IsCompiled() const override;
		virtual const std::unordered_map<std::string, ShaderBuffer>&			  GetBuffers() const override { return m_Buffers; }
		virtual const std::unordered_map<std::string, ShaderResourceDeclaration>& GetResources() const override { return m_Resources; }
		virtual const std::unordered_map<std::string, SpecializationCache>&		  GetSpecializationCachce() const override { return m_Specializations; }


		const std::vector<DescriptorSet>&				    GetDescriptorSets()		 const { return m_DescriptorSets; }
		const std::vector<PushConstantRange>&			    GetPushConstantRanges()	 const { return m_PushConstantRanges; }
		const std::vector<VkPipelineShaderStageCreateInfo>& GetPipelineShaderStageCreateInfos() const { return m_PipelineShaderStageCreateInfos; }
		const VkPipelineShaderStageCreateInfo*				GetPipelineVertexShaderStageCreateInfo() const;
		const VkWriteDescriptorSet*						    GetDescriptorSet(const std::string& name, uint32_t set) const;
		std::pair<const VkWriteDescriptorSet*, uint32_t>    GetDescriptorSet(const std::string& name) const;

		const VkWriteDescriptorSet*						    TryGetDescriptorSet(const std::string& name, uint32_t set) const;
		std::pair<const VkWriteDescriptorSet*, uint32_t>    TryGetDescriptorSet(const std::string& name) const;

		std::vector<VkDescriptorSetLayout>				    GetAllDescriptorSetLayouts() const;
		
	private:
		
		struct PreprocessData
		{
			StageMap<std::string> Sources;
			StageMap<std::unordered_map<std::string, ShaderParser::ShaderLayoutInfo>> LayoutInfo;
		};

		bool			compileOrGetVulkanBinaries(const StageMap<std::string>& sources, StageMap<std::vector<uint32_t>>& output, bool forceCompile);
		PreprocessData	preProcess(const std::string& source) const;
		void			createProgram(const StageMap<std::vector<uint32_t>>& shaderData);
		
		void reflectAllStages(const StageMap<std::vector<uint32_t>>& shaderData, const PreprocessData& preprocessData);
		void reflectStage(VkShaderStageFlagBits stage, const std::vector<uint32_t>& shaderData, const PreprocessData& preprocessData);
		void reflectConstantBuffers(const spirv_cross::Compiler& compiler, VkShaderStageFlagBits stage, spirv_cross::SmallVector<spirv_cross::Resource>& buffers);
		void reflectStorageBuffers(const spirv_cross::Compiler& compiler, VkShaderStageFlagBits stage, spirv_cross::SmallVector<spirv_cross::Resource>& buffers);
		void reflectUniformBuffers(const spirv_cross::Compiler& compiler, VkShaderStageFlagBits stage, spirv_cross::SmallVector<spirv_cross::Resource>& buffers);
		void reflectSampledImages(const spirv_cross::Compiler& compiler, VkShaderStageFlagBits stage, spirv_cross::SmallVector<spirv_cross::Resource>& sampledImages);
		void reflectStorageImages(const spirv_cross::Compiler& compiler, VkShaderStageFlagBits stage, spirv_cross::SmallVector<spirv_cross::Resource>& storageImages);
		void reflectSpecializationConstants(const spirv_cross::Compiler& compiler, VkShaderStageFlagBits stage, spirv_cross::SmallVector<spirv_cross::SpecializationConstant>& specializationConstants);

		void   createDescriptorSetLayout();
		void   destroy();	
		bool   binaryExists(const StageMap<std::string>& sources) const;
		size_t getBuffersSize() const;
	private:
		bool					   m_Compiled;
		std::string				   m_Name;
		std::string				   m_FilePath;
		std::string				   m_Source;
		mutable ShaderParser	   m_Parser;
		size_t					   m_SourceHash;

		StageMap<std::vector<uint32_t>> m_ShaderData;

		std::unordered_map<std::string, SpecializationCache> m_Specializations;

		std::vector<DescriptorSet> m_DescriptorSets;
		std::vector<BufferLayout>  m_Layouts;

		std::vector<VkPipelineShaderStageCreateInfo>				m_PipelineShaderStageCreateInfos;
	
		std::unordered_map<std::string, ShaderResourceDeclaration>	m_Resources;
		std::vector<PushConstantRange>								m_PushConstantRanges;
		
		std::unordered_map<std::string, ShaderBuffer>				m_Buffers;
		size_t														m_VertexBufferSize;
	};
}