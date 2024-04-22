#include "stdafx.h"
#include "VulkanRendererAPI.h"

#include "VulkanRenderCommandBuffer.h"
#include "VulkanPipeline.h"
#include "VulkanPipelineCompute.h"
#include "VulkanRenderPass.h"
#include "VulkanFramebuffer.h"
#include "VulkanContext.h"
#include "VulkanVertexBuffer.h"
#include "VulkanIndexBuffer.h"
#include "VulkanUniformBuffer.h"
#include "VulkanUniformBufferSet.h"
#include "VulkanStorageBufferSet.h"

#include "VulkanMaterial.h"
#include "VulkanImage.h"

#include "XYZ/Core/Application.h"
#include "XYZ/Debug/Profiler.h"

namespace XYZ {
	namespace Utils {

		static const char* VulkanVendorIDToString(uint32_t vendorID)
		{
			switch (vendorID)
			{
			case 0x10DE: return "NVIDIA";
			case 0x1002: return "AMD";
			case 0x8086: return "INTEL";
			case 0x13B5: return "ARM";
			}
			return "Unknown";
		}
	}

	static Ref<VulkanDescriptorAllocator> s_DescriptorAllocator;


	VulkanRendererAPI::~VulkanRendererAPI()
	{
		s_DescriptorAllocator.Reset();
	}

	void VulkanRendererAPI::Init()
	{
		s_DescriptorAllocator = Ref<VulkanDescriptorAllocator>::Create();
		s_DescriptorAllocator->Init();
		auto device = VulkanContext::GetCurrentDevice()->GetPhysicalDevice();
		auto& properties = device->GetProperties();
		auto& memoryProperties = device->GetMemoryProperties();
		auto& caps = RendererAPI::getCapabilities();

		caps.Vendor = Utils::VulkanVendorIDToString(properties.vendorID);
		caps.Device = properties.deviceName;
		caps.Version = std::to_string(properties.driverVersion);
	}

	void VulkanRendererAPI::Shutdown()
	{
		s_DescriptorAllocator->Shutdown();	
	}

	void VulkanRendererAPI::BeginFrame()
	{
		s_DescriptorAllocator->TryResetFull();
	}

	void VulkanRendererAPI::EndFrame()
	{
		Renderer::Submit([]()
		{
		});
	}

	void VulkanRendererAPI::BeginRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer,
		Ref<RenderPass> renderPass, bool subPass, bool explicitClear)
	{
		Renderer::Submit([renderCommandBuffer, renderPass, subPass, explicitClear]() mutable
		{
			XYZ_PROFILE_FUNC("VulkanRendererAPI::BeginRenderPass");
			Ref<VulkanContext> vulkanContext = Renderer::GetAPIContext();
			Ref<VulkanFramebuffer> framebuffer = renderPass->GetSpecification().TargetFramebuffer;
			if (framebuffer->GetSpecification().SwapChainTarget)
			{
				const VulkanSwapChain& swapChain = vulkanContext->GetSwapChain();
				framebuffer = swapChain.GetRenderPass()->GetSpecification().TargetFramebuffer.As<VulkanFramebuffer>();
			}
			const uint32_t frameIndex = vulkanContext->GetSwapChain().GetCurrentBufferIndex();
			uint32_t width = framebuffer->GetSpecification().Width;
			uint32_t height = framebuffer->GetSpecification().Height;

			VkViewport viewport = { 0.0f, static_cast<float>(height) };
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			viewport.width = static_cast<float>(width);
			viewport.height = -static_cast<float>(height);

			VkRenderPassBeginInfo renderPassBeginInfo = {};
			renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassBeginInfo.pNext = nullptr;
			renderPassBeginInfo.renderPass = framebuffer->GetRenderPass();
			renderPassBeginInfo.renderArea.offset = { 0, 0 };
			renderPassBeginInfo.renderArea.extent.width = width;
			renderPassBeginInfo.renderArea.extent.height = height;
			renderPassBeginInfo.framebuffer = framebuffer->GetFramebuffer();
			
			renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(framebuffer->GetVulkanClearValues().size());
			renderPassBeginInfo.pClearValues = framebuffer->GetVulkanClearValues().data();
		
			VkRect2D scissor = {};
			scissor.offset = { 0, 0 };
			scissor.extent.width = width;
			scissor.extent.height = height;

			if (framebuffer->GetSpecification().SwapChainTarget)
			{
				const VulkanSwapChain& swapChain = vulkanContext->GetSwapChain();
				const auto extent = swapChain.GetExtent();
				renderPassBeginInfo.renderArea.extent = extent;
				renderPassBeginInfo.framebuffer = swapChain.GetCurrentFramebuffer();
				renderPassBeginInfo.renderPass = swapChain.GetVulkanRenderPass();
				
				viewport = { 0.0f, static_cast<float>(extent.height) };
				viewport.width = static_cast<float>(extent.width);
				viewport.height = -static_cast<float>(extent.height);
				scissor.extent = extent;
			}

			const VkCommandBuffer commandBuffer = (const VkCommandBuffer)renderCommandBuffer->CommandBufferHandle(frameIndex);

			const VkSubpassContents contents = subPass ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE;
			vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, contents);
			
			// We use secondary command buffer
			if (!subPass)
			{
				if (explicitClear)
					clearFramebuffer(framebuffer, commandBuffer);

				// Update dynamic viewport state
				vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
				// Update dynamic scissor state				
				vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
			}
		});
	}

	void VulkanRendererAPI::EndRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer)
	{
		Renderer::Submit([renderCommandBuffer]() mutable
		{
			XYZ_PROFILE_FUNC("VulkanRendererAPI::EndRenderPass");
			const uint32_t frameIndex = VulkanContext::Get()->GetSwapChain().GetCurrentBufferIndex();

			const VkCommandBuffer commandBuffer = (const VkCommandBuffer)renderCommandBuffer->CommandBufferHandle(frameIndex);
		
			vkCmdEndRenderPass(commandBuffer);
		});
	}

	void VulkanRendererAPI::RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline,
		Ref<MaterialInstance> material, Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const PushConstBuffer& constData, uint32_t indexCount, uint32_t vertexOffsetSize)
	{
		XYZ_ASSERT(material.Raw(), "");

		PushConstBuffer fsUniformStorage = material->GetFSUniformsBuffer();
		PushConstBuffer vsUniformStorage = material->GetVSUniformsBuffer();

		Renderer::Submit([renderCommandBuffer, pipeline, material, vertexBuffer, indexBuffer, vsData = constData, indexCount, vertexOffsetSize, fsUniformStorage]() mutable
		{
			XYZ_PROFILE_FUNC("VulkanRendererAPI::RenderGeometry");
			const VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			const uint32_t frameIndex = VulkanContext::Get()->GetSwapChain().GetCurrentBufferIndex();

			Ref<VulkanIndexBuffer>		   vulkanIndexBuffer = indexBuffer;
			Ref<VulkanShader>			   vulkanShader = pipeline->GetSpecification().Shader;
			Ref<VulkanPipeline>			   vulkanPipeline = pipeline;

			const VkCommandBuffer		   commandBuffer = (const VkCommandBuffer)renderCommandBuffer->CommandBufferHandle(frameIndex);
			const VkPipelineLayout		   layout = vulkanPipeline->GetVulkanPipelineLayout();


			///////////////////////////////		
			const VkBuffer vertexBuffers[] = { vertexBuffer.As<VulkanVertexBuffer>()->GetVulkanBuffer() };
			const VkDeviceSize offsets[] = { vertexOffsetSize };
			if (indexCount == 0)
				indexCount = indexBuffer->GetUseCount();
			uint32_t offset = 0;
			VkDeviceSize sizes[] = { vertexBuffer->GetUseSize() - vertexOffsetSize };

			vkCmdBindVertexBuffers2(commandBuffer, 0, 1, vertexBuffers, offsets, sizes, nullptr);
			vkCmdBindIndexBuffer(commandBuffer, vulkanIndexBuffer->GetVulkanBuffer(), offset, vulkanIndexBuffer->GetVulkanIndexType());
	
			if (vsData.Size)
				vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, vsData.Size, vsData.Bytes);
			if (fsUniformStorage.Size)
				vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, vsData.Size, fsUniformStorage.Size, fsUniformStorage.Bytes);
			vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
		});
	}

	void VulkanRendererAPI::RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, 
		Ref<MaterialInstance> material, Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, uint32_t indexCount)
	{
		XYZ_ASSERT(material.Raw(), "");

		PushConstBuffer fsUniformStorage = material->GetFSUniformsBuffer();
		PushConstBuffer vsUniformStorage = material->GetVSUniformsBuffer();

		Renderer::Submit([renderCommandBuffer, pipeline, material, vertexBuffer, indexBuffer, fsUniformStorage, vsUniformStorage]() mutable
		{
			XYZ_PROFILE_FUNC("VulkanRendererAPI::RenderGeometry");
			const VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			const uint32_t frameIndex = VulkanContext::Get()->GetSwapChain().GetCurrentBufferIndex();
			
			Ref<VulkanIndexBuffer>		   vulkanIndexBuffer = indexBuffer;
			Ref<VulkanShader>			   vulkanShader = pipeline->GetSpecification().Shader;
			Ref<VulkanPipeline>			   vulkanPipeline = pipeline;

			const VkCommandBuffer		   commandBuffer = (const VkCommandBuffer)renderCommandBuffer->CommandBufferHandle(frameIndex);
			const VkPipelineLayout		   layout = vulkanPipeline->GetVulkanPipelineLayout();


			///////////////////////////////		
			const VkBuffer vertexBuffers[] = { vertexBuffer.As<VulkanVertexBuffer>()->GetVulkanBuffer() };
			const VkDeviceSize offsets[] = { 0 };
			const VkDeviceSize sizes[] = { vertexBuffer->GetUseSize() };
			vkCmdBindVertexBuffers2(commandBuffer, 0, 1, vertexBuffers, offsets, sizes, nullptr);
			vkCmdBindIndexBuffer(commandBuffer, vulkanIndexBuffer->GetVulkanBuffer(), 0, vulkanIndexBuffer->GetVulkanIndexType());
			
			glm::mat4 trans(1.0f);
			vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &trans);
			if (fsUniformStorage.Size)
				vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, vsUniformStorage.Size, fsUniformStorage.Size, fsUniformStorage.Bytes);
			vkCmdDrawIndexed(commandBuffer, indexBuffer->GetUseCount(), 1, 0, 0, 0);
		});
	}

	void VulkanRendererAPI::RenderMesh(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material, Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const PushConstBuffer& constData)
	{
		XYZ_ASSERT(material.Raw(), "");

		PushConstBuffer fsUniformStorage = material->GetFSUniformsBuffer();
		Renderer::Submit([renderCommandBuffer, pipeline, material, vertexBuffer, indexBuffer, vsData = constData, fsUniformStorage]() mutable
		{
			XYZ_PROFILE_FUNC("VulkanRendererAPI::RenderGeometry");
			const VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			const uint32_t frameIndex = VulkanContext::Get()->GetSwapChain().GetCurrentBufferIndex();
			
			Ref<VulkanVertexBuffer>		   vulkanVertexBuffer = vertexBuffer.As<VulkanVertexBuffer>();
			Ref<VulkanIndexBuffer>		   vulkanIndexBuffer = indexBuffer;
			Ref<VulkanShader>			   vulkanShader = pipeline->GetSpecification().Shader;
			Ref<VulkanPipeline>			   vulkanPipeline = pipeline;

			const VkCommandBuffer		   commandBuffer = (const VkCommandBuffer)renderCommandBuffer->CommandBufferHandle(frameIndex);
			const VkPipelineLayout		   layout = vulkanPipeline->GetVulkanPipelineLayout();


			///////////////////////////////		
			// Vertex Buffer
			VkBuffer vbVertexBuffer = vulkanVertexBuffer->GetVulkanBuffer();
			VkDeviceSize offsets[1] = { 0 };
			VkDeviceSize sizes[1] = { vulkanVertexBuffer->GetUseSize() };


			vkCmdBindVertexBuffers2(commandBuffer, 0, 1, &vbVertexBuffer, offsets, sizes, nullptr);
			// Index buffer
			vkCmdBindIndexBuffer(commandBuffer, vulkanIndexBuffer->GetVulkanBuffer(), 0, vulkanIndexBuffer->GetVulkanIndexType());

			if (vsData.Size)
				vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, vsData.Size, vsData.Bytes);
			if (fsUniformStorage.Size)
				vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, vsData.Size, fsUniformStorage.Size, fsUniformStorage.Bytes);

			vkCmdDrawIndexed(commandBuffer, indexBuffer->GetUseCount(), 1, 0, 0, 0);
		});
	}

	
	void VulkanRendererAPI::RenderMesh(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, 
		Ref<MaterialInstance> material, Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const PushConstBuffer& constData, Ref<VertexBufferSet> instanceBuffer, uint32_t instanceOffset, uint32_t instanceCount)
	{
		XYZ_ASSERT(material.Raw(), "");

		PushConstBuffer fsUniformStorage = material->GetFSUniformsBuffer();

		Renderer::Submit([renderCommandBuffer, pipeline, material, 
			vertexBuffer, indexBuffer, vsData = constData,
			instanceBuffer, instanceOffset, instanceCount,
			fsUniformStorage
		]() mutable
		{
			XYZ_PROFILE_FUNC("VulkanRendererAPI::RenderMesh");
			const VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			const uint32_t frameIndex = VulkanContext::Get()->GetSwapChain().GetCurrentBufferIndex();
			
			Ref<VulkanVertexBuffer>		   vulkanVertexBuffer = vertexBuffer.As<VulkanVertexBuffer>();
			Ref<VulkanVertexBuffer>		   vulkanInstanceBuffer = instanceBuffer->GetVertexBuffer(frameIndex).As<VulkanVertexBuffer>();
			Ref<VulkanIndexBuffer>		   vulkanIndexBuffer = indexBuffer;

			Ref<VulkanShader>			   vulkanShader = pipeline->GetSpecification().Shader;
			Ref<VulkanPipeline>			   vulkanPipeline = pipeline;


			const VkCommandBuffer		   commandBuffer = (const VkCommandBuffer)renderCommandBuffer->CommandBufferHandle(frameIndex);
			const VkPipelineLayout		   layout = vulkanPipeline->GetVulkanPipelineLayout();


			///////////////////////////////		
			// Vertex Buffer
			VkBuffer vbVertexBuffer = vulkanVertexBuffer->GetVulkanBuffer();
			VkDeviceSize offsets[1] = { 0 };
			VkDeviceSize sizes[1] = { vulkanVertexBuffer->GetUseSize() };
			vkCmdBindVertexBuffers2(commandBuffer, 0, 1, &vbVertexBuffer, offsets, sizes, nullptr);
			
			// Instance Buffer
			VkBuffer vbInstanceBuffer = vulkanInstanceBuffer->GetVulkanBuffer();
			VkDeviceSize instanceOffsets[1] = { instanceOffset };
			VkDeviceSize instanceBufferSizes[1] = { vulkanInstanceBuffer->GetUseSize() - instanceOffset };
			vkCmdBindVertexBuffers2(commandBuffer, 1, 1, &vbInstanceBuffer, instanceOffsets, instanceBufferSizes, nullptr);

			// Index buffer
			vkCmdBindIndexBuffer(commandBuffer, vulkanIndexBuffer->GetVulkanBuffer(), 0, vulkanIndexBuffer->GetVulkanIndexType());

			if (vsData.Size)
				vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, vsData.Size, vsData.Bytes);
			if (fsUniformStorage.Size)
				vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, vsData.Size, fsUniformStorage.Size, fsUniformStorage.Bytes);

			vkCmdDrawIndexed(commandBuffer, indexBuffer->GetUseCount(), instanceCount, 0, 0, 0);
		});
	}

	void VulkanRendererAPI::RenderMesh(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material, Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const PushConstBuffer& constData, Ref<StorageBufferSet> instanceBuffer, uint32_t set, uint32_t binding, uint32_t instanceOffset, uint32_t instanceCount)
	{
		XYZ_ASSERT(material.Raw(), "");

		PushConstBuffer fsUniformStorage = material->GetFSUniformsBuffer();

		Renderer::Submit([renderCommandBuffer, pipeline, material,
			vertexBuffer, indexBuffer, vsData = constData,
			instanceBuffer, instanceOffset, instanceCount, binding, set,
			fsUniformStorage
		]() mutable
			{
				XYZ_PROFILE_FUNC("VulkanRendererAPI::RenderMesh");
				const VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
				const uint32_t frameIndex = VulkanContext::Get()->GetSwapChain().GetCurrentBufferIndex();

				Ref<VulkanVertexBuffer>		   vulkanVertexBuffer = vertexBuffer.As<VulkanVertexBuffer>();
				Ref<VulkanStorageBuffer>	   vulkanInstanceBuffer = instanceBuffer->Get(binding, set, frameIndex).As<VulkanStorageBuffer>();
				Ref<VulkanIndexBuffer>		   vulkanIndexBuffer = indexBuffer;

				Ref<VulkanShader>			   vulkanShader = pipeline->GetSpecification().Shader;
				Ref<VulkanPipeline>			   vulkanPipeline = pipeline;


				const VkCommandBuffer		   commandBuffer = (const VkCommandBuffer)renderCommandBuffer->CommandBufferHandle(frameIndex);
				const VkPipelineLayout		   layout = vulkanPipeline->GetVulkanPipelineLayout();


				///////////////////////////////		
				// Vertex Buffer
				VkBuffer vbVertexBuffer = vulkanVertexBuffer->GetVulkanBuffer();
				VkDeviceSize offsets[1] = { 0 };
				VkDeviceSize sizes[1] = { vulkanVertexBuffer->GetUseSize() };
				vkCmdBindVertexBuffers2(commandBuffer, 0, 1, &vbVertexBuffer, offsets, sizes, nullptr);

				// Instance Buffer
				VkBuffer vbInstanceBuffer = vulkanInstanceBuffer->GetVulkanBuffer();
				VkDeviceSize instanceOffsets[1] = { instanceOffset };
				VkDeviceSize instanceBufferSizes[1] = { vulkanInstanceBuffer->GetSize() - instanceOffset};
				vkCmdBindVertexBuffers2(commandBuffer, 1, 1, &vbInstanceBuffer, instanceOffsets, instanceBufferSizes, nullptr);

				// Index buffer
				vkCmdBindIndexBuffer(commandBuffer, vulkanIndexBuffer->GetVulkanBuffer(), 0, vulkanIndexBuffer->GetVulkanIndexType());

				if (vsData.Size)
					vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, vsData.Size, vsData.Bytes);
				if (fsUniformStorage.Size)
					vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, vsData.Size, fsUniformStorage.Size, fsUniformStorage.Bytes);

				vkCmdDrawIndexed(commandBuffer, indexBuffer->GetUseCount(), instanceCount, 0, 0, 0);
			});
	}

	void VulkanRendererAPI::RenderMesh(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, 
		Ref<MaterialInstance> material, Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, Ref<VertexBufferSet> transformBuffer, uint32_t transformOffset, uint32_t transformInstanceCount, Ref<VertexBufferSet> instanceBuffer, uint32_t instanceOffset, uint32_t instanceCount)
	{
		PushConstBuffer fsUniformStorage = material->GetFSUniformsBuffer();
		PushConstBuffer vsUniformStorage = material->GetVSUniformsBuffer();

		Renderer::Submit([renderCommandBuffer, pipeline, material,
			vertexBuffer, indexBuffer,
			transformBuffer, transformOffset, transformInstanceCount,
			instanceBuffer, instanceOffset, instanceCount,
			fsUniformStorage, vsUniformStorage
		]() mutable
		{
			XYZ_PROFILE_FUNC("VulkanRendererAPI::RenderMesh");
			const VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			const uint32_t frameIndex = VulkanContext::Get()->GetSwapChain().GetCurrentBufferIndex();
			
			Ref<VulkanVertexBuffer>		   vulkanVertexBuffer = vertexBuffer.As<VulkanVertexBuffer>();
			Ref<VulkanVertexBuffer>		   vulkanTransformBuffer = transformBuffer->GetVertexBuffer(frameIndex).As<VulkanVertexBuffer>();
			Ref<VulkanVertexBuffer>		   vulkanInstanceBuffer = instanceBuffer->GetVertexBuffer(frameIndex).As<VulkanVertexBuffer>();
			Ref<VulkanIndexBuffer>		   vulkanIndexBuffer = indexBuffer;

			Ref<VulkanShader>			   vulkanShader = pipeline->GetSpecification().Shader;
			Ref<VulkanPipeline>			   vulkanPipeline = pipeline;


			const VkCommandBuffer		   commandBuffer = (const VkCommandBuffer)renderCommandBuffer->CommandBufferHandle(frameIndex);
			const VkPipelineLayout		   layout = vulkanPipeline->GetVulkanPipelineLayout();


			///////////////////////////////		
			// Vertex Buffer
			VkBuffer vbVertexBuffer = vulkanVertexBuffer->GetVulkanBuffer();
			VkDeviceSize offsets[1] = { 0 };
			VkDeviceSize sizes[1] = { vulkanVertexBuffer->GetUseSize() };
			vkCmdBindVertexBuffers2(commandBuffer, 0, 1, &vbVertexBuffer, offsets, sizes, nullptr);

			// Vertex Transform Buffer
			VkBuffer vbTransformBuffer = vulkanTransformBuffer->GetVulkanBuffer();
			VkDeviceSize transformInstanceOffsets[1] = { transformOffset };
			VkDeviceSize transformBufferSizes[1] = { vulkanTransformBuffer->GetUseSize() - transformOffset };
			vkCmdBindVertexBuffers2(commandBuffer, 1, 1, &vbTransformBuffer, transformInstanceOffsets, transformBufferSizes, nullptr);

			// Instance Buffer
			VkBuffer vbInstanceBuffer = vulkanInstanceBuffer->GetVulkanBuffer();
			VkDeviceSize instanceOffsets[1] = { instanceOffset };
			VkDeviceSize instanceBufferSizes[1] = { vulkanInstanceBuffer->GetUseSize() - instanceOffset };
			vkCmdBindVertexBuffers2(commandBuffer, 2, 1, &vbInstanceBuffer, instanceOffsets, instanceBufferSizes, nullptr);

			// Index buffer
			vkCmdBindIndexBuffer(commandBuffer, vulkanIndexBuffer->GetVulkanBuffer(), 0, vulkanIndexBuffer->GetVulkanIndexType());
			
			
			glm::mat4 trans(1.0f);
			vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &trans);
			if (fsUniformStorage.Size != 0)
				vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, vsUniformStorage.Size, fsUniformStorage.Size, fsUniformStorage.Bytes);

			vkCmdDrawIndexed(commandBuffer, indexBuffer->GetUseCount(), instanceCount, 0, 0, 0);
		});
	}

	void VulkanRendererAPI::RenderIndirect(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material,
		Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const PushConstBuffer& constData,
		Ref<StorageBufferSet> indirectBuffer, uint32_t indirectOffset, uint32_t indirectCount, uint32_t indirectStride
	)
	{
		XYZ_ASSERT(material.Raw(), "");

		PushConstBuffer fsUniformStorage = material->GetFSUniformsBuffer();

		Renderer::Submit([renderCommandBuffer, pipeline, material,
			vertexBuffer,indexBuffer, vsData = constData,
			indirectBuffer, indirectOffset, indirectCount, indirectStride,
			fsUniformStorage
		]() mutable
		{
				XYZ_PROFILE_FUNC("VulkanRendererAPI::RenderIndirect");
				const VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
				const uint32_t frameIndex = VulkanContext::Get()->GetSwapChain().GetCurrentBufferIndex();

				
				Ref<VulkanVertexBuffer>		   vulkanVertexBuffer = vertexBuffer.As<VulkanVertexBuffer>();
				Ref<VulkanIndexBuffer>		   vulkanIndexBuffer = indexBuffer;

				Ref<VulkanShader>			   vulkanShader = pipeline->GetSpecification().Shader;
				Ref<VulkanPipeline>			   vulkanPipeline = pipeline;


				const VkCommandBuffer		   commandBuffer = (const VkCommandBuffer)renderCommandBuffer->CommandBufferHandle(frameIndex);
				const VkPipelineLayout		   layout = vulkanPipeline->GetVulkanPipelineLayout();


				///////////////////////////////		
				// Vertex Buffer
				VkBuffer vbVertexBuffer = vulkanVertexBuffer->GetVulkanBuffer();
				VkDeviceSize offsets[1] = { 0 };
				VkDeviceSize sizes[1] = { vulkanVertexBuffer->GetUseSize() };
				vkCmdBindVertexBuffers2(commandBuffer, 0, 1, &vbVertexBuffer, offsets, sizes, nullptr);

				// Index buffer
				vkCmdBindIndexBuffer(commandBuffer, vulkanIndexBuffer->GetVulkanBuffer(), 0, vulkanIndexBuffer->GetVulkanIndexType());

				if (vsData.Size)
					vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, vsData.Size, vsData.Bytes);
				if (fsUniformStorage.Size)
					vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, vsData.Size, fsUniformStorage.Size, fsUniformStorage.Bytes);


				const auto& indirectBuffers = indirectBuffer.As<VulkanStorageBufferSet>()->GetIndirect()[frameIndex];
				for (auto& buffer : indirectBuffers)
				{
					vkCmdDrawIndexedIndirect(commandBuffer, buffer->GetVulkanBuffer(), indirectOffset, indirectCount, indirectStride);
				}
				
		});
	}


	void VulkanRendererAPI::BindPipeline(
		Ref<RenderCommandBuffer> renderCommandBuffer, 
		Ref<Pipeline> pipeline, 
		Ref<UniformBufferSet> uniformBufferSet, 
		Ref<StorageBufferSet> storageBufferSet, 
		Ref<Material> material
	)
	{
		Renderer::Submit([renderCommandBuffer, pipeline, uniformBufferSet, storageBufferSet, material]() mutable
		{
			XYZ_PROFILE_FUNC("VulkanRendererAPI::BindPipeline");
			const VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			const uint32_t frameIndex = VulkanContext::Get()->GetSwapChain().GetCurrentBufferIndex();

			Ref<VulkanShader>			   vulkanShader = pipeline->GetSpecification().Shader;
			Ref<VulkanPipeline>			   vulkanPipeline = pipeline;
			Ref<VulkanUniformBufferSet>	   vulkanUniformBufferSet = uniformBufferSet;
			Ref<VulkanStorageBufferSet>    vulkanStorageBufferSet = storageBufferSet;
			Ref<VulkanMaterial>			   vulkanMaterial = material;
			

			const VkCommandBuffer		   commandBuffer = (const VkCommandBuffer)renderCommandBuffer->CommandBufferHandle(frameIndex);
			const VkPipelineLayout		   layout = vulkanPipeline->GetVulkanPipelineLayout();

	
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.As<VulkanPipeline>()->GetVulkanPipeline());

			vulkanMaterial->RT_UpdateForRendering(vulkanUniformBufferSet, vulkanStorageBufferSet);

			const auto& materialDescriptors = vulkanMaterial->GetDescriptors(frameIndex);
			if (!materialDescriptors.empty())
			{
				vkCmdBindDescriptorSets(
					commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout,
					0, static_cast<uint32_t>(materialDescriptors.size()),
					materialDescriptors.data(), 0, nullptr
				);
			}
			if (vulkanPipeline->GetSpecification().Topology == PrimitiveTopology::Lines)
				vkCmdSetLineWidth(commandBuffer, vulkanPipeline->GetSpecification().LineWidth);
		});
	}	

	void VulkanRendererAPI::BeginPipelineCompute(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<PipelineCompute> pipeline, Ref<UniformBufferSet> uniformBufferSet, Ref<StorageBufferSet> storageBufferSet, Ref<Material> material)
	{
		Renderer::Submit([renderCommandBuffer, pipeline, uniformBufferSet, storageBufferSet, material]() mutable
		{
			XYZ_PROFILE_FUNC("VulkanRendererAPI::BeginPipelineCompute");
			const VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			const uint32_t frameIndex = VulkanContext::Get()->GetSwapChain().GetCurrentBufferIndex();

			
			Ref<VulkanShader>			   vulkanShader = pipeline->GetShader();
			Ref<VulkanPipelineCompute>	   vulkanPipeline = pipeline;
			Ref<VulkanUniformBufferSet>	   vulkanUniformBufferSet = uniformBufferSet;
			Ref<VulkanStorageBufferSet>    vulkanStorageBufferSet = storageBufferSet;
			Ref<VulkanMaterial>			   vulkanMaterial = material;

			const VkCommandBuffer		   commandBuffer = (const VkCommandBuffer)renderCommandBuffer->CommandBufferHandle(frameIndex);
			const VkPipelineLayout		   layout = vulkanPipeline->GetVulkanPipelineLayout();

			vulkanPipeline->Begin(renderCommandBuffer);
			vulkanMaterial->RT_UpdateForRendering(vulkanUniformBufferSet, vulkanStorageBufferSet);

			const auto& materialDescriptors = vulkanMaterial->GetDescriptors(frameIndex);

			vkCmdBindDescriptorSets(
				commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, layout,
				0, static_cast<uint32_t>(materialDescriptors.size()),
				materialDescriptors.data(), 0, nullptr
			);
		});
	}

	

	void VulkanRendererAPI::DispatchCompute(Ref<PipelineCompute> pipeline, Ref<MaterialInstance> material, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ, const PushConstBuffer& constData)
	{
		PushConstBuffer vsUniformStorage = constData;
		if (material.Raw())
			vsUniformStorage += material->GetVSUniformsBuffer();

		Renderer::Submit([pipeline, material, groupCountX, groupCountY, groupCountZ, vsUniformStorage]() {
			Ref<VulkanPipelineCompute> vulkanPipeline = pipeline;

			if (vsUniformStorage.Size != 0)
			{
				vkCmdPushConstants(
					vulkanPipeline->GetActiveCommandBuffer(),
					vulkanPipeline->GetVulkanPipelineLayout(),
					VK_SHADER_STAGE_COMPUTE_BIT, 0, vsUniformStorage.Size, vsUniformStorage.Bytes
				);
			}
			vulkanPipeline->Dispatch(groupCountX, groupCountY, groupCountZ);
		});
	}

	void VulkanRendererAPI::EndPipelineCompute(Ref<PipelineCompute> pipeline)
	{
		Renderer::Submit([pipeline]() mutable {
			pipeline->End();
		});
	}

	void VulkanRendererAPI::UpdateDescriptors(Ref<PipelineCompute> pipeline, Ref<Material> material, Ref<UniformBufferSet> uniformBufferSet, Ref<StorageBufferSet> storageBufferSet)
	{
		Renderer::Submit([pipeline, material, uniformBufferSet, storageBufferSet]() {
			
			const uint32_t frameIndex = VulkanContext::Get()->GetSwapChain().GetCurrentBufferIndex();


			Ref<VulkanUniformBufferSet>	   vulkanUniformBufferSet = uniformBufferSet;
			Ref<VulkanStorageBufferSet>    vulkanStorageBufferSet = storageBufferSet;
			Ref<VulkanMaterial>			   vulkanMaterial = material;
			Ref<VulkanShader>			   vulkanShader = pipeline->GetShader();
			Ref<VulkanPipelineCompute>	   vulkanPipeline = pipeline;

			const VkCommandBuffer		   commandBuffer = vulkanPipeline->GetActiveCommandBuffer();
			const VkPipelineLayout		   layout = vulkanPipeline->GetVulkanPipelineLayout();

			vulkanMaterial->RT_UpdateForRendering(vulkanUniformBufferSet, vulkanStorageBufferSet, true);
		
			const auto& materialDescriptors = vulkanMaterial->GetDescriptors(frameIndex);

			vkCmdBindDescriptorSets(
				commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, layout,
				0, static_cast<uint32_t>(materialDescriptors.size()),
				materialDescriptors.data(), 0, nullptr
			);
		});
	}

	void VulkanRendererAPI::UpdateDescriptors(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<Material> material, Ref<UniformBufferSet> uniformBufferSet, Ref<StorageBufferSet> storageBufferSet)
	{
		Renderer::Submit([renderCommandBuffer, pipeline, material, uniformBufferSet, storageBufferSet]() {

			const uint32_t frameIndex = VulkanContext::Get()->GetSwapChain().GetCurrentBufferIndex();


			Ref<VulkanUniformBufferSet>	   vulkanUniformBufferSet = uniformBufferSet;
			Ref<VulkanStorageBufferSet>    vulkanStorageBufferSet = storageBufferSet;
			Ref<VulkanMaterial>			   vulkanMaterial = material;
			Ref<VulkanPipeline>			   vulkanPipeline = pipeline;

			const VkCommandBuffer		   commandBuffer = (const VkCommandBuffer)renderCommandBuffer->CommandBufferHandle(frameIndex);
			const VkPipelineLayout		   layout = vulkanPipeline->GetVulkanPipelineLayout();

			vulkanMaterial->RT_UpdateForRendering(vulkanUniformBufferSet, vulkanStorageBufferSet, true);

			const auto& materialDescriptors = vulkanMaterial->GetDescriptors(frameIndex);

			if (!materialDescriptors.empty())
			{
				vkCmdBindDescriptorSets(
					commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout,
					0, static_cast<uint32_t>(materialDescriptors.size()),
					materialDescriptors.data(), 0, nullptr
				);
			}
		});
	}

	void VulkanRendererAPI::ClearImage(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Image2D> image)
	{
		Renderer::Submit([renderCommandBuffer, image = image.As<VulkanImage2D>()]() mutable
			{
				const VkCommandBuffer commandBuffer = (const VkCommandBuffer)renderCommandBuffer->CommandBufferHandle(Renderer::GetCurrentFrame());

				VkImageSubresourceRange subresourceRange{};
				subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				subresourceRange.baseMipLevel = 0;
				subresourceRange.levelCount = image->GetSpecification().Mips;
				subresourceRange.layerCount = image->GetSpecification().Layers;

				VkClearColorValue clearColor{ 0.f, 0.f, 0.f, 0.f };
				vkCmdClearColorImage(commandBuffer, image->GetImageInfo().Image, image->GetSpecification().Usage == ImageUsage::Storage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, &clearColor, 1, &subresourceRange);
			});
	}


	VkDescriptorSet VulkanRendererAPI::RT_AllocateDescriptorSet(VkDescriptorSetAllocateInfo& allocInfo)
	{
		XYZ_PROFILE_FUNC("VulkanRendererAPI::RT_AllocateDescriptorSet");
		return s_DescriptorAllocator->RT_Allocate(allocInfo);
	}

	VkDescriptorSet VulkanRendererAPI::RT_AllocateDescriptorSet(const VkDescriptorSetLayout& layout)
	{	
		return s_DescriptorAllocator->RT_Allocate(layout);
	}

	VulkanDescriptorAllocator::Version VulkanRendererAPI::GetDescriptorAllocatorVersion(uint32_t frame)
	{
		return s_DescriptorAllocator->GetVersion(frame);
	}


	void VulkanRendererAPI::InsertImageMemoryBarrier(VkCommandBuffer cmdbuffer, VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageSubresourceRange subresourceRange)
	{
		VkImageMemoryBarrier imageMemoryBarrier{};
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		imageMemoryBarrier.srcAccessMask = srcAccessMask;
		imageMemoryBarrier.dstAccessMask = dstAccessMask;
		imageMemoryBarrier.oldLayout = oldImageLayout;
		imageMemoryBarrier.newLayout = newImageLayout;
		imageMemoryBarrier.image = image;
		imageMemoryBarrier.subresourceRange = subresourceRange;

		vkCmdPipelineBarrier(
			cmdbuffer,
			srcStageMask,
			dstStageMask,
			0,
			0, nullptr,
			0, nullptr,
			1, &imageMemoryBarrier);
	}

	void VulkanRendererAPI::SetImageLayout(VkCommandBuffer cmdbuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkImageSubresourceRange subresourceRange, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
	{
		// Create an image barrier object
		VkImageMemoryBarrier imageMemoryBarrier = {};
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.oldLayout = oldImageLayout;
		imageMemoryBarrier.newLayout = newImageLayout;
		imageMemoryBarrier.image = image;
		imageMemoryBarrier.subresourceRange = subresourceRange;

		// Source layouts (old)
		// Source access mask controls actions that have to be finished on the old layout
		// before it will be transitioned to the new layout
		switch (oldImageLayout)
		{
		case VK_IMAGE_LAYOUT_UNDEFINED:
			// Image layout is undefined (or does not matter)
			// Only valid as initial layout
			// No flags required, listed only for completeness
			imageMemoryBarrier.srcAccessMask = 0;
			break;

		case VK_IMAGE_LAYOUT_PREINITIALIZED:
			// Image is preinitialized
			// Only valid as initial layout for linear images, preserves memory contents
			// Make sure host writes have been finished
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			break;

		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			// Image is a color attachment
			// Make sure any writes to the color buffer have been finished
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;

		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			// Image is a depth/stencil attachment
			// Make sure any writes to the depth/stencil buffer have been finished
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;

		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			// Image is a transfer source
			// Make sure any reads from the image have been finished
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;

		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			// Image is a transfer destination
			// Make sure any writes to the image have been finished
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;

		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			// Image is read by a shader
			// Make sure any shader reads from the image have been finished
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		default:
			// Other source layouts aren't handled (yet)
			break;
		}

		// Target layouts (new)
		// Destination access mask controls the dependency for the new image layout
		switch (newImageLayout)
		{
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			// Image will be used as a transfer destination
			// Make sure any writes to the image have been finished
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;

		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			// Image will be used as a transfer source
			// Make sure any reads from the image have been finished
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;

		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			// Image will be used as a color attachment
			// Make sure any writes to the color buffer have been finished
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;

		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			// Image layout will be used as a depth/stencil attachment
			// Make sure any writes to depth/stencil buffer have been finished
			imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;

		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			// Image will be read in a shader (sampler, input attachment)
			// Make sure any writes to the image have been finished
			if (imageMemoryBarrier.srcAccessMask == 0)
			{
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
			}
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		default:
			// Other source layouts aren't handled (yet)
			break;
		}

		// Put barrier inside setup command buffer
		vkCmdPipelineBarrier(
			cmdbuffer,
			srcStageMask,
			dstStageMask,
			0,
			0, nullptr,
			0, nullptr,
			1, &imageMemoryBarrier);
	}

	void VulkanRendererAPI::SetImageLayout(VkCommandBuffer cmdbuffer, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
	{
		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = aspectMask;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = 1;
		subresourceRange.layerCount = 1;
		SetImageLayout(cmdbuffer, image, oldImageLayout, newImageLayout, subresourceRange, srcStageMask, dstStageMask);

	}

	void VulkanRendererAPI::clearFramebuffer(Ref<VulkanFramebuffer> framebuffer, VkCommandBuffer commandBuffer)
	{
		const uint32_t colorAttachmentCount = framebuffer->GetNumColorAttachments();
		const auto& clearValues = framebuffer->GetVulkanClearValues();
	

		const uint32_t width = framebuffer->GetSpecification().Width;
		const uint32_t height = framebuffer->GetSpecification().Height;
		std::vector<VkClearAttachment> attachments(colorAttachmentCount);
		std::vector<VkClearRect> clearRects(colorAttachmentCount);
		for (uint32_t i = 0; i < colorAttachmentCount; i++)
		{
			attachments[i].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			attachments[i].colorAttachment = i;
			attachments[i].clearValue = clearValues[i];

			clearRects[i].rect.offset = { 0, 0 };
			clearRects[i].rect.extent = { width, height };
			clearRects[i].baseArrayLayer = 0;
			clearRects[i].layerCount = 1;
		}

		if (framebuffer->HasDepthAttachment())
		{
			Ref<VulkanImage2D> depthImage = framebuffer->GetDepthImage();
			attachments.push_back({});
			clearRects.push_back({});
			attachments[colorAttachmentCount].aspectMask = depthImage->GetImageViewAspectFlags();
			attachments[colorAttachmentCount].clearValue = clearValues[colorAttachmentCount];
			clearRects[colorAttachmentCount].rect.offset = { 0, 0 };
			clearRects[colorAttachmentCount].rect.extent = { width, height };
			clearRects[colorAttachmentCount].baseArrayLayer = 0;
			clearRects[colorAttachmentCount].layerCount = 1;
		}

		vkCmdClearAttachments(commandBuffer, attachments.size(), attachments.data(), clearRects.size(), clearRects.data());
	}
}
