#pragma once
#include "APIContext.h"
#include "XYZ/Core/Ref/Ref.h"
#include "XYZ/Renderer/Pipeline.h"
#include "XYZ/Renderer/PipelineCompute.h"
#include "XYZ/Renderer/RenderCommandBuffer.h"
#include "XYZ/Renderer/UniformBufferSet.h"
#include "XYZ/Renderer/StorageBufferSet.h"
#include "XYZ/Renderer/VertexBufferSet.h"
#include "XYZ/Renderer/Material.h"
#include "XYZ/Renderer/PushConstBuffer.h"

namespace XYZ {

	enum class PrimitiveType
	{
		None,
		Triangles,
		Lines,
		Points
	};

	struct RenderAPICapabilities
	{
		std::string Vendor;
		std::string Device;
		std::string Version;

		int   MaxSamples = 0;
		float MaxAnisotropy = 0.0f;
		int   MaxTextureUnits = 0;
	};

	

	class XYZ_API RendererAPI
	{
	public:
		enum class Type
		{
			None = 0, Vulkan = 1
		};
	public:
		virtual ~RendererAPI() = default;
		virtual void Init() = 0;
		virtual void Shutdown() = 0;

		virtual void BeginFrame() = 0;
		virtual void EndFrame() = 0;
		virtual void BeginRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RenderPass> renderPass, bool subPass = false, bool explicitClear = false) {};
		virtual void EndRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer) {};
		
		virtual void RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material, Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, 
			const PushConstBuffer& constData, uint32_t indexCount = 0, uint32_t vertexOffsetSize = 0) {};

		virtual void RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material, Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, uint32_t indexCount = 0) {};

		virtual void RenderMesh(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material,
			Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const PushConstBuffer& constData
		) {};
		virtual void RenderMesh(
			Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material,
			Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const PushConstBuffer& constData,
			Ref<VertexBufferSet> instanceBuffer, uint32_t instanceOffset, uint32_t instanceCount
		) {};

		virtual void RenderMesh(
			Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material,
			Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const PushConstBuffer& constData,
			Ref<StorageBufferSet> instanceBuffer, uint32_t set, uint32_t binding, uint32_t instanceOffset, uint32_t instanceCount
		) {};

		virtual void RenderMesh(
			Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material,
			Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer,
			Ref<VertexBufferSet> transformBuffer, uint32_t transformOffset, uint32_t transformInstanceCount,
			Ref<VertexBufferSet> instanceBuffer, uint32_t instanceOffset, uint32_t instanceCount
		) {};

		virtual void RenderIndirect(
			Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material,
			Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const PushConstBuffer& constData,
			Ref<StorageBufferSet> indirectBuffer, uint32_t indirectOffset, uint32_t indirectCount, uint32_t indirectStride
		) {};

		virtual void BindPipeline(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<UniformBufferSet> uniformBufferSet, Ref<StorageBufferSet> storageBufferSet, Ref<Material> material) {};
		virtual void BeginPipelineCompute(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<PipelineCompute> pipeline, Ref<UniformBufferSet> uniformBufferSet, Ref<StorageBufferSet> storageBufferSet, Ref<Material> material) {};
		
		virtual void DispatchCompute(Ref<PipelineCompute> pipeline, Ref<MaterialInstance> material, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ, const PushConstBuffer& constData) {};
		virtual void EndPipelineCompute(Ref<PipelineCompute> pipeline) {};
		virtual void UpdateDescriptors(Ref<PipelineCompute> pipeline, Ref<Material> material, Ref<UniformBufferSet> uniformBufferSet, Ref<StorageBufferSet> storageBufferSet) {};
		virtual void UpdateDescriptors(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<Material> material, Ref<UniformBufferSet> uniformBufferSet, Ref<StorageBufferSet> storageBufferSet) {};
		virtual void ClearImage(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Image2D> image) {};

		static const RenderAPICapabilities& GetCapabilities();
		

		static Type GetType() { return s_API; }
		
	protected:
		static RenderAPICapabilities& getCapabilities();


	private:
		static Type s_API;

	};
	
}