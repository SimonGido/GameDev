#pragma once
#include "XYZ/Renderer/RenderCommandBuffer.h"

#include <vulkan/vulkan.h>

namespace XYZ {
	class VulkanSwapChain;
	class VulkanRenderCommandBuffer : public RenderCommandBuffer
	{
	public:
		VulkanRenderCommandBuffer(uint32_t count = 0, std::string debugName = "");
		VulkanRenderCommandBuffer(std::string debugName, VulkanSwapChain& swapChain);

		~VulkanRenderCommandBuffer() override;

		virtual void Begin() override;
		virtual void End() override;
		virtual void Submit() override;

		virtual float GetExecutionGPUTime(uint32_t frameIndex, uint32_t queryIndex = 0) const override;

		virtual uint64_t BeginTimestampQuery() override;
		virtual void EndTimestampQuery(uint64_t queryID) override;

		VkCommandBuffer GetVulkanCommandBuffer(uint32_t index) const { return m_CommandBuffers[index]; }
	private:
		
	private:
		std::string					 m_Name;
		VkCommandPool				 m_CommandPool;
		VulkanSwapChain*			 m_SwapChain;
		std::vector<VkCommandBuffer> m_CommandBuffers;
		bool						 m_OwnedBySwapchain;

		friend class VulkanSwapChain;
	};
}