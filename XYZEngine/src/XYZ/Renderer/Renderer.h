#pragma once
#include <memory>

#include "XYZ/Core/Application.h"
#include "XYZ/Core/ThreadPool.h"
#include "XYZ/Utils/DataStructures/ThreadPass.h"
#include "XYZ/Debug/Profiler.h"

#include "Shader.h"
#include "Camera.h"

#include "RendererAPI.h"
#include "RenderCommandQueue.h"
#include "RenderPass.h"
#include "Pipeline.h"
#include "PipelineCompute.h"
#include "Material.h"
#include "StorageBufferSet.h"
#include "VertexBufferSet.h"
#include "ShaderIncluder.h"

#include "XYZ/Asset/Renderer/MaterialAsset.h"


namespace XYZ {

	struct RendererStats
	{
		RendererStats();
		void Reset();

		uint32_t DrawArraysCount;
		uint32_t DrawIndexedCount;
		uint32_t DrawInstancedCount;
		uint32_t DrawFullscreenCount;
		uint32_t DrawIndirectCount;

		uint32_t CommandsCount;
	};

	struct XYZ_API RendererResources
	{
		void Init();
		void Shutdown();

		ShaderIncluder Includer;
		std::unordered_map<std::string, Ref<Asset>> RendererAssets;
	};

	struct RendererConfiguration
	{
		uint32_t FramesInFlight = 3;
	};



	class XYZ_API Renderer
	{
	public:
		static void Init(const RendererConfiguration& config = RendererConfiguration());
		static void InitAPI(bool initDefaultResources = true);

		static void Shutdown();

		static void BeginFrame();
		static void EndFrame();

		static void BeginRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer, const Ref<RenderPass>& renderPass, bool subPass, bool clear);
		static void EndRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer);
		
		static void RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline,  Ref<MaterialInstance> material, Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, 
			const PushConstBuffer& constData, uint32_t indexCount = 0, uint32_t vertexOffsetSize = 0);
		
		static void RenderMesh(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material,
			Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const PushConstBuffer& constData
		);
		
		static void RenderMesh(
			Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material,
			Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const PushConstBuffer& constData,
			Ref<VertexBufferSet> instanceBuffer, uint32_t instanceOffset, uint32_t instanceCount
		);
		
		static void RenderMesh(
			Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material,
			Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const PushConstBuffer& constData,
			uint32_t instanceOffset, uint32_t instanceCount
		);

		static void RenderMesh(
			Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material,
			Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer,
			Ref<VertexBufferSet> transformBuffer, uint32_t transformOffset, uint32_t transformInstanceCount,
			Ref<VertexBufferSet> instanceBuffer, uint32_t instanceOffset, uint32_t instanceCount
		);

		static void RenderIndirectMesh(
			Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material,
			Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const PushConstBuffer& constData,
			Ref<StorageBufferSet> indirectBuffer, uint32_t indirectOffset, uint32_t indirectCount, uint32_t indirectStride
		);

		static void SubmitFullscreenQuad(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material);
		static void SubmitFullscreenQuad(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material, const PushConstBuffer& constData);
		
		static void BindPipeline(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<UniformBufferSet> uniformBufferSet, Ref<StorageBufferSet> storageBufferSet, Ref<Material> material);	
		static void BeginPipelineCompute(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<PipelineCompute> pipeline, Ref<UniformBufferSet> uniformBufferSet, Ref<StorageBufferSet> storageBufferSet, Ref<Material> material);

		static void DispatchCompute(Ref<PipelineCompute> pipeline, Ref<MaterialInstance> material, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ, const PushConstBuffer& constData = {});
		static void EndPipelineCompute(Ref<PipelineCompute> pipeline);
		static void UpdateDescriptors(Ref<PipelineCompute> pipeline, Ref<Material> material, Ref<UniformBufferSet> uniformBufferSet, Ref<StorageBufferSet> storageBufferSet);
		static void UpdateDescriptors(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<Material> material, Ref<UniformBufferSet> uniformBufferSet, Ref<StorageBufferSet> storageBufferSet);

		static void ClearImage(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Image2D> image);


		static void RegisterShaderDependency(const Ref<Shader>& shader, const Ref<PipelineCompute>& pipeline);
		static void RegisterShaderDependency(const Ref<Shader>& shader, const Ref<Pipeline>& pipeline);
		static void RegisterShaderDependency(const Ref<Shader>& shader, const Ref<Material>& material);
		

		static void RemoveShaderDependency(size_t hash);
		static void OnShaderReload(size_t hash);


		//////////////
		static Ref<APIContext>				GetAPIContext();
		
		static const RendererResources&		GetDefaultResources();
		static const RenderAPICapabilities& GetCapabilities();
		static const RendererConfiguration& GetConfiguration();

		template <typename FuncT>
		static void Submit(RenderCommandQueue& queue, FuncT&& func);

		template<typename FuncT>
		static void Submit(FuncT&& func);

		template <typename FuncT>
		static void SubmitResource(FuncT&& func);

		template<typename FuncT>
		static void SubmitAndWait(FuncT&& func);


		static void BlockRenderThread();
		static void WaitAndRenderAll();
		static void WaitAndRender();

		static void Render();
		static void ExecuteResources();
		static void ExecuteResources(uint32_t index);

		static ThreadPool&			GetPool();
		static RendererAPI::Type	GetAPI() { return RendererAPI::GetType(); }
		static const RendererStats& GetStats();
		static uint32_t			    GetCurrentFrame();

	private:
		static ScopedLock<RenderCommandQueue> getResourceQueue();
		static ScopedLock<RenderCommandQueue> getRenderCommandQueue();
		static RendererStats&				  getStats();
	};

	template<typename FuncT>
	inline void Renderer::Submit(RenderCommandQueue& queue, FuncT&& func)
	{
		XYZ_PROFILE_FUNC("Renderer::Submit");

		auto renderCmd = [](void* ptr) {

			auto pFunc = static_cast<FuncT*>(ptr);
			(*pFunc)();
			pFunc->~FuncT(); // Call destructor
		};

		auto storageBuffer = queue.Allocate(renderCmd, sizeof(func));
		new (storageBuffer) FuncT(std::forward<FuncT>(func));
		getStats().CommandsCount++;
	}

	template <typename FuncT>
	void Renderer::Submit(FuncT&& func)
	{
		XYZ_PROFILE_FUNC("Renderer::Submit");

		auto renderCmd = [](void* ptr) {

			auto pFunc = static_cast<FuncT*>(ptr);
			(*pFunc)();
			pFunc->~FuncT(); // Call destructor
		};

		ScopedLock<RenderCommandQueue> queue = getRenderCommandQueue();
		auto storageBuffer = queue->Allocate(renderCmd, sizeof(func));
		new (storageBuffer) FuncT(std::forward<FuncT>(func));
		getStats().CommandsCount++;
	}

	template<typename FuncT>
	inline void Renderer::SubmitResource(FuncT&& func)
	{
		XYZ_PROFILE_FUNC("Renderer::SubmitResource");
		auto renderCmd = [](void* ptr) {

			auto pFunc = static_cast<FuncT*>(ptr);
			(*pFunc)();
			pFunc->~FuncT(); // Call destructor
		};
		ScopedLock<RenderCommandQueue> resourceQueue = getResourceQueue();
		auto storageBuffer = resourceQueue->Allocate(renderCmd, sizeof(func));
		new (storageBuffer) FuncT(std::forward<FuncT>(func));
	}

	template <typename FuncT>
	void Renderer::SubmitAndWait(FuncT&& func)
	{
		Submit(std::forward<FuncT>(func));
		WaitAndRender();
		BlockRenderThread();
	}

	namespace Utils {

		inline void DumpGPUInfo()
		{
			auto& caps = Renderer::GetCapabilities();
			XYZ_TRACE("GPU Info:");
			XYZ_TRACE("  Vendor: {0}", caps.Vendor);
			XYZ_TRACE("  Device: {0}", caps.Device);
			XYZ_TRACE("  Version: {0}", caps.Version);
		}
	}


}