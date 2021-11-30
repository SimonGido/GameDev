#pragma once
#include "XYZ/Renderer/RenderCommandBuffer.h"

#include <vulkan/vulkan.h>

namespace XYZ {
	class VulkanSwapChain;
	class VulkanRenderCommandBuffer : public RenderCommandBuffer
	{
	public:
		VulkanRenderCommandBuffer(uint32_t count = 0, std::string debugName = "");
		VulkanRenderCommandBuffer(std::string debugName);

		~VulkanRenderCommandBuffer() override;

		virtual void Begin() override;
		virtual void End() override;
		virtual void Submit() override;
		virtual void RT_Begin() override;
		virtual void RT_End() override;
		virtual void CreatePipelineStatisticsQueries(uint32_t count) override;
		virtual void CreateTimestampQueries(uint32_t count) override;

		virtual float GetExecutionGPUTime(uint32_t frameIndex, uint32_t queryIndex = 0) const override;
		virtual const PipelineStatistics& GetPipelineStatistics(uint32_t frameIndex) const override { return m_PipelineStatisticsQueryResults[frameIndex]; }
		
		virtual uint64_t BeginTimestampQuery() override;
		virtual void EndTimestampQuery(uint64_t queryID) override;

		VkCommandBuffer GetVulkanCommandBuffer(uint32_t index) const { return m_CommandBuffers[index]; }
	private:
		void createTimestampQueries(uint32_t count);
		void createPipelineStatisticsQueries(uint32_t count);

		void destroyTimestampQueries();
		void destroyPipelineStatisticsQueries();
	private:
		std::string						   m_Name;
		VkCommandPool					   m_CommandPool;
		std::vector<VkCommandBuffer>	   m_CommandBuffers;
		std::vector<VkFence>			   m_WaitFences;
		bool							   m_OwnedBySwapchain;

		
		uint32_t						   m_TimestampQueryCount = 0;
		uint32_t						   m_TimestampNextAvailableQuery = 2;
		std::vector<VkQueryPool>		   m_TimestampQueryPools;
		std::vector<std::vector<uint64_t>> m_TimestampQueryResults;
		std::vector<std::vector<float>>    m_ExecutionGPUTimes;


		uint32_t						   m_PipelineQueryCount = 0;
		std::vector<VkQueryPool>		   m_PipelineStatisticsQueryPools;
		std::vector<PipelineStatistics>    m_PipelineStatisticsQueryResults;

		friend class VulkanSwapChain;
	};
}