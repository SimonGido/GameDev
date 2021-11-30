#include "stdafx.h"
#include "VulkanRenderCommandBuffer.h"

#include "VulkanContext.h"
#include "VulkanSwapChain.h"

namespace XYZ {
	VulkanRenderCommandBuffer::VulkanRenderCommandBuffer(uint32_t count, std::string debugName)
		:
		m_Name(std::move(debugName)),
		m_CommandBuffers(VK_NULL_HANDLE),
		m_CommandPool(VK_NULL_HANDLE),
		m_OwnedBySwapchain(false)
	{
		auto device = VulkanContext::GetCurrentDevice();
		uint32_t framesInFlight = Renderer::GetConfiguration().FramesInFlight;


		VkCommandPoolCreateInfo cmdPoolInfo = {};
		cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmdPoolInfo.queueFamilyIndex = device->GetPhysicalDevice()->GetQueueFamilyIndices().Graphics;
		cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VK_CHECK_RESULT(vkCreateCommandPool(device->GetVulkanDevice(), &cmdPoolInfo, nullptr, &m_CommandPool));

		VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
		commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocateInfo.commandPool = m_CommandPool;
		commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		if (count == 0)
			count = framesInFlight;
		commandBufferAllocateInfo.commandBufferCount = count;
		m_CommandBuffers.resize(count);
		VK_CHECK_RESULT(vkAllocateCommandBuffers(device->GetVulkanDevice(), &commandBufferAllocateInfo, m_CommandBuffers.data()));



		VkFenceCreateInfo fenceCreateInfo{};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		m_WaitFences.resize(framesInFlight);
		for (auto& fence : m_WaitFences)
			VK_CHECK_RESULT(vkCreateFence(device->GetVulkanDevice(), &fenceCreateInfo, nullptr, &fence));
	}

	VulkanRenderCommandBuffer::VulkanRenderCommandBuffer(std::string debugName)
		:
		m_Name(std::move(debugName)),
		m_CommandPool(VK_NULL_HANDLE),
		m_OwnedBySwapchain(true)
	{
		const VulkanSwapChain& swapChain = VulkanContext::GetSwapChain();
		auto device = swapChain.GetDevice();
		m_CommandBuffers.resize(swapChain.GetNumCommandsBuffers());

		for (size_t frame = 0; frame < swapChain.GetNumCommandsBuffers(); frame++)
			m_CommandBuffers[frame] = swapChain.GetCommandBuffer(frame);
	}
	VulkanRenderCommandBuffer::~VulkanRenderCommandBuffer()
	{
		destroyTimestampQueries();
		destroyPipelineStatisticsQueries();
		if (m_OwnedBySwapchain)
			return;

		VkCommandPool commandPool = m_CommandPool;
		auto fences = m_WaitFences;
		Renderer::SubmitResource([commandPool, fences]()
		{
			auto device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

			for (const auto fence : fences)
				vkDestroyFence(device, fence, nullptr);

			vkDestroyCommandPool(device, commandPool, nullptr);
		});
	}
	void VulkanRenderCommandBuffer::Begin()
	{
		Ref<VulkanRenderCommandBuffer> instance = this;
		Renderer::Submit([instance]() mutable {
			instance->RT_Begin();
		});
	}
	void VulkanRenderCommandBuffer::End()
	{
		Ref<VulkanRenderCommandBuffer> instance = this;
		Renderer::Submit([instance]()mutable {
			instance->RT_End();
		});
	}
	void VulkanRenderCommandBuffer::Submit()
	{
		if (m_OwnedBySwapchain)
			return;

		Ref<VulkanRenderCommandBuffer> instance = this;
		Renderer::Submit([instance]() mutable
		{
			auto device = VulkanContext::GetCurrentDevice();

			uint32_t frameIndex = Renderer::GetAPIContext()->GetCurrentFrame();

			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			VkCommandBuffer commandBuffer = instance->m_CommandBuffers[frameIndex];
			submitInfo.pCommandBuffers = &commandBuffer;

			VK_CHECK_RESULT(vkWaitForFences(device->GetVulkanDevice(), 1, &instance->m_WaitFences[frameIndex], VK_TRUE, UINT64_MAX));
			VK_CHECK_RESULT(vkResetFences(device->GetVulkanDevice(), 1, &instance->m_WaitFences[frameIndex]));
			VK_CHECK_RESULT(vkQueueSubmit(device->GetGraphicsQueue(), 1, &submitInfo, instance->m_WaitFences[frameIndex]));
		});
	}

	void VulkanRenderCommandBuffer::RT_Begin()
	{
		const uint32_t frameIndex = VulkanContext::Get()->GetCurrentFrame();
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;; // Optional
		beginInfo.pInheritanceInfo = nullptr; // Optional

		const VkCommandBuffer commandBuffer = m_CommandBuffers[frameIndex];
		VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &beginInfo));

		// Timestamp query
		if (!m_TimestampQueryPools.empty())
		{
			vkCmdResetQueryPool(commandBuffer, m_TimestampQueryPools[frameIndex], 0, m_TimestampQueryCount);
			vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_TimestampQueryPools[frameIndex], 0);
		}
		// Pipeline stats query
		if (!m_PipelineStatisticsQueryPools.empty())
		{
			vkCmdResetQueryPool(commandBuffer, m_PipelineStatisticsQueryPools[frameIndex], 0, m_PipelineQueryCount);
			vkCmdBeginQuery(commandBuffer, m_PipelineStatisticsQueryPools[frameIndex], 0, 0);
		}
	}

	void VulkanRenderCommandBuffer::RT_End() 
	{
		const uint32_t frameIndex = VulkanContext::Get()->GetCurrentFrame();

     	const VkCommandBuffer commandBuffer = m_CommandBuffers[frameIndex];
		if (!m_TimestampQueryPools.empty())
		{
			vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_TimestampQueryPools[frameIndex], 1);
		}
		if (!m_PipelineStatisticsQueryPools.empty())
		{
			vkCmdEndQuery(commandBuffer, m_PipelineStatisticsQueryPools[frameIndex], 0);
		}
		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));
	}

	void VulkanRenderCommandBuffer::CreatePipelineStatisticsQueries(uint32_t count)
	{
		destroyPipelineStatisticsQueries();
		createPipelineStatisticsQueries(count);
	}

	void VulkanRenderCommandBuffer::CreateTimestampQueries(uint32_t count)
	{
		destroyTimestampQueries();
		createTimestampQueries(count);
	}

	float VulkanRenderCommandBuffer::GetExecutionGPUTime(uint32_t frameIndex, uint32_t queryIndex) const
	{
		if (queryIndex / 2 >= m_TimestampNextAvailableQuery / 2)
			return 0.0f;

		return m_ExecutionGPUTimes[frameIndex][queryIndex / 2];
	}
	uint64_t VulkanRenderCommandBuffer::BeginTimestampQuery()
	{
		uint64_t queryIndex = m_TimestampNextAvailableQuery;
		m_TimestampNextAvailableQuery += 2;
		Ref<VulkanRenderCommandBuffer> instance = this;
		Renderer::Submit([instance, queryIndex]()
		{
			uint32_t frameIndex = Renderer::GetCurrentFrame();
			VkCommandBuffer commandBuffer = instance->m_CommandBuffers[frameIndex];
			vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, instance->m_TimestampQueryPools[frameIndex], queryIndex);
		});
		return queryIndex;
	}
	void VulkanRenderCommandBuffer::EndTimestampQuery(uint64_t queryID)
	{
		Ref<VulkanRenderCommandBuffer> instance = this;
		Renderer::Submit([instance, queryID]()
		{
			uint32_t frameIndex = Renderer::GetCurrentFrame();
			VkCommandBuffer commandBuffer = instance->m_CommandBuffers[frameIndex];
			vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, instance->m_TimestampQueryPools[frameIndex], queryID + 1);
		});
	}

	void VulkanRenderCommandBuffer::createTimestampQueries(uint32_t count)
	{
		auto device = VulkanContext::GetCurrentDevice();
		uint32_t framesInFlight = Renderer::GetConfiguration().FramesInFlight;
		VkQueryPoolCreateInfo queryPoolCreateInfo = {};
		queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		queryPoolCreateInfo.pNext = nullptr;

		m_TimestampQueryCount = 2 + 2 *count;

		queryPoolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
		queryPoolCreateInfo.queryCount = m_TimestampQueryCount;
		m_TimestampQueryPools.resize(framesInFlight);
		for (auto& timestampQueryPool : m_TimestampQueryPools)
			VK_CHECK_RESULT(vkCreateQueryPool(device->GetVulkanDevice(), &queryPoolCreateInfo, nullptr, &timestampQueryPool));

		m_TimestampQueryResults.resize(framesInFlight);
		for (auto& timestampQueryResults : m_TimestampQueryResults)
			timestampQueryResults.resize(m_TimestampQueryCount);

		m_ExecutionGPUTimes.resize(framesInFlight);
		for (auto& executionGPUTimes : m_ExecutionGPUTimes)
			executionGPUTimes.resize(m_TimestampQueryCount / 2);
	}

	void VulkanRenderCommandBuffer::createPipelineStatisticsQueries(uint32_t count)
	{
		auto device = VulkanContext::GetCurrentDevice();
		uint32_t framesInFlight = Renderer::GetConfiguration().FramesInFlight;

		VkQueryPoolCreateInfo queryPoolCreateInfo = {};
		queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		queryPoolCreateInfo.pNext = nullptr;

		m_PipelineQueryCount = count;
		queryPoolCreateInfo.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
		queryPoolCreateInfo.queryCount = m_PipelineQueryCount;
		queryPoolCreateInfo.pipelineStatistics =
			VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
			VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
			VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
			VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
			VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
			VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
			VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;

		m_PipelineStatisticsQueryPools.resize(framesInFlight);
		for (auto& pipelineStatisticsQueryPools : m_PipelineStatisticsQueryPools)
			VK_CHECK_RESULT(vkCreateQueryPool(device->GetVulkanDevice(), &queryPoolCreateInfo, nullptr, &pipelineStatisticsQueryPools));

		m_PipelineStatisticsQueryResults.resize(framesInFlight);
	}

	void VulkanRenderCommandBuffer::destroyTimestampQueries()
	{
		if (m_TimestampQueryPools.empty())
			return;

		auto timestampQueryPools = m_TimestampQueryPools;
		Renderer::SubmitResource([timestampQueryPools]()
		{
			auto device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

			for (const auto query : timestampQueryPools)
				vkDestroyQueryPool(device, query, nullptr);

		});
	}

	void VulkanRenderCommandBuffer::destroyPipelineStatisticsQueries()
	{
		if (m_PipelineStatisticsQueryPools.empty())
			return;

		auto pipelineStatisticsQueryPools = m_PipelineStatisticsQueryPools;
		Renderer::SubmitResource([ pipelineStatisticsQueryPools]()
		{
			auto device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			for (const auto query : pipelineStatisticsQueryPools)
				vkDestroyQueryPool(device, query, nullptr);
		});
	}

}