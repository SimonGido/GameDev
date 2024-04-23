#pragma once
#include "VulkanFramebuffer.h"
#include "VulkanDescriptorAllocator.h"
#include "XYZ/Renderer/RendererAPI.h"

namespace XYZ {

	class VulkanRendererAPI : public RendererAPI
	{
	public:
		virtual ~VulkanRendererAPI() override;
		virtual void Init() override;
		virtual void Shutdown() override;
		virtual void BeginFrame() override;
		virtual void EndFrame() override;

		virtual void BeginRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RenderPass> renderPass, bool subPass = false, bool explicitClear = false) override;
		virtual void EndRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer) override;
		
		virtual void RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material, Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const PushConstBuffer& constData, uint32_t indexCount = 0, uint32_t vertexOffsetSize = 0) override;
		virtual void RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material, Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, uint32_t indexCount = 0) override;
		virtual void RenderMesh(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material,
			Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const PushConstBuffer& constData
		) override;
		virtual void RenderMesh(
			Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material, 
			Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const PushConstBuffer& constData,
			Ref<VertexBufferSet> instanceBuffer, uint32_t instanceOffset, uint32_t instanceCount
		) override;

		virtual void RenderMesh(
			Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material,
			Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const PushConstBuffer& constData,
			uint32_t instanceOffset, uint32_t instanceCount
		) override;

		virtual void RenderMesh(
			Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material,
			Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer,
			Ref<VertexBufferSet> transformBuffer, uint32_t transformOffset, uint32_t transformInstanceCount,
			Ref<VertexBufferSet> instanceBuffer, uint32_t instanceOffset, uint32_t instanceCount
		) override;


		virtual void RenderIndirect(
			Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material,
			Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const PushConstBuffer& constData,
			Ref<StorageBufferSet> indirectBuffer, uint32_t indirectOffset, uint32_t indirectCount, uint32_t indirectStride
		) override;

		virtual void BindPipeline(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<UniformBufferSet> uniformBufferSet, Ref<StorageBufferSet> storageBufferSet, Ref<Material> material) override;
		virtual void BeginPipelineCompute(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<PipelineCompute> pipeline, Ref<UniformBufferSet> uniformBufferSet, Ref<StorageBufferSet> storageBufferSet, Ref<Material> material) override;
		
		virtual void DispatchCompute(Ref<PipelineCompute> pipeline, Ref<MaterialInstance> material, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ, const PushConstBuffer& constData) override;
		virtual void EndPipelineCompute(Ref<PipelineCompute> pipeline) override;
		virtual void UpdateDescriptors(Ref<PipelineCompute> pipeline, Ref<Material> material, Ref<UniformBufferSet> uniformBufferSet, Ref<StorageBufferSet> storageBufferSet) override;
		virtual void UpdateDescriptors(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<Material> material, Ref<UniformBufferSet> uniformBufferSet, Ref<StorageBufferSet> storageBufferSet) override;

		virtual void ClearImage(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Image2D> image) override;

		static VkDescriptorSet					  RT_AllocateDescriptorSet(VkDescriptorSetAllocateInfo& allocInfo);
		static VkDescriptorSet					  RT_AllocateDescriptorSet(const VkDescriptorSetLayout& layout);
		static VulkanDescriptorAllocator::Version GetDescriptorAllocatorVersion(uint32_t frame);

		static void InsertImageMemoryBarrier(
				VkCommandBuffer cmdbuffer,
				VkImage image,
				VkAccessFlags srcAccessMask,
				VkAccessFlags dstAccessMask,
				VkImageLayout oldImageLayout,
				VkImageLayout newImageLayout,
				VkPipelineStageFlags srcStageMask,
				VkPipelineStageFlags dstStageMask,
				VkImageSubresourceRange subresourceRange
		);

		static void SetImageLayout(
				VkCommandBuffer cmdbuffer,
				VkImage image,
				VkImageLayout oldImageLayout,
				VkImageLayout newImageLayout,
				VkImageSubresourceRange subresourceRange,
				VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
		);

		static void SetImageLayout(
				VkCommandBuffer cmdbuffer,
				VkImage image,
				VkImageAspectFlags aspectMask,
				VkImageLayout oldImageLayout,
				VkImageLayout newImageLayout,
				VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
		);

	private:
		static void clearFramebuffer(Ref<VulkanFramebuffer> framebuffer, VkCommandBuffer commandBuffer);
	};
}