#include "stdafx.h"
#include "Renderer.h"


#include "Renderer2D.h"
#include "SceneRenderer.h"
#include "Fence.h"
#include "RendererQueueData.h"

#include "XYZ/Core/Application.h"
#include "XYZ/Debug/Profiler.h"

#include "XYZ/Asset/AssetManager.h"

#include "XYZ/API/Vulkan/VulkanRendererAPI.h"

namespace XYZ {


	struct ShaderDependencies
	{
		void Clear()
		{
			MaterialDependencies.clear();
			PipelineDependencies.clear();
			PipelineComputeDependencies.clear();
		}
		std::vector<Ref<Material>>		    MaterialDependencies;
		std::vector<Ref<Pipeline>>		    PipelineDependencies;
		std::vector<Ref<PipelineCompute>>   PipelineComputeDependencies;
	};

	struct ShaderDependencyMap
	{
		void OnReload(size_t hash)
		{
			std::scoped_lock lock(m_Mutex);
			auto it = m_Dependencies.find(hash);
			if (it != m_Dependencies.end())
			{
				for (auto& material : it->second.MaterialDependencies)
					material->Invalidate();
				for (auto& pipeline : it->second.PipelineDependencies)
					pipeline->Invalidate();
				for (auto& pipeline : it->second.PipelineComputeDependencies)
					pipeline->Invalidate();
			}
		}
		void Register(size_t hash, const Ref<Material>& material)
		{
			std::scoped_lock lock(m_Mutex);
			m_Dependencies[hash].MaterialDependencies.push_back(material);
		}
		void Register(size_t hash, const Ref<Pipeline>& pipeline)
		{
			std::scoped_lock lock(m_Mutex);
			m_Dependencies[hash].PipelineDependencies.push_back(pipeline);
		}
		void Register(size_t hash, const Ref<PipelineCompute>& pipeline)
		{
			std::scoped_lock lock(m_Mutex);
			m_Dependencies[hash].PipelineComputeDependencies.push_back(pipeline);
		}
		void RemoveDependency(size_t hash)
		{
			ShaderDependencies dependency;
			{
				std::scoped_lock lock(m_Mutex);
				auto it = m_Dependencies.find(hash);
				if (it != m_Dependencies.end())
				{
					dependency = std::move(it->second);
					m_Dependencies.erase(it);
				}
			}
			dependency.Clear();
		}

		void Clear()
		{
			while (!m_Dependencies.empty())
			{
				RemoveDependency(m_Dependencies.begin()->first);
			}
		}
	private:
		std::unordered_map<size_t, ShaderDependencies> m_Dependencies;
		std::mutex m_Mutex;
	};

	

	struct RendererData
	{
		RendererQueueData			   QueueData;
		Ref<APIContext>				   APIContext;
		Ref<VertexBuffer>			   FullscreenQuadVertexBuffer;
		Ref<IndexBuffer>			   FullscreenQuadIndexBuffer;

		RendererStats				   Stats;
		RendererConfiguration		   Configuration;
		RendererResources			   Resources;
		ShaderDependencyMap			   ShaderDependencies;
	};

	RendererAPI::Type RendererAPI::s_API = RendererAPI::Type::Vulkan;

	static RendererData s_Data;
	static RendererAPI* s_RendererAPI = nullptr;


	static RendererAPI* CreateRendererAPI()
	{
		switch (RendererAPI::GetType())
		{
		case RendererAPI::Type::Vulkan: return new VulkanRendererAPI();
		}
		XYZ_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}

	static void SetupFullscreenQuad()
	{
		const float x = -1;
		const float y = -1;
		const float width = 2, height = 2;
		struct QuadVertex
		{
			glm::vec3 Position;
			glm::vec2 TexCoord;
		};

		QuadVertex* data = new QuadVertex[4];

		data[0].Position = glm::vec3(x, y, 3.0f);
		data[0].TexCoord = glm::vec2(0, 1);

		data[1].Position = glm::vec3(x + width, y, 2.0f);
		data[1].TexCoord = glm::vec2(1, 1);

		data[2].Position = glm::vec3(x + width, y + height, 1.0f);
		data[2].TexCoord = glm::vec2(1, 0);

		data[3].Position = glm::vec3(x, y + height, 0.0f);
		data[3].TexCoord = glm::vec2(0, 0);


		s_Data.FullscreenQuadVertexBuffer = VertexBuffer::Create(data, 4 * sizeof(QuadVertex));
	
		const uint32_t indices[6] = { 0, 1, 2, 2, 3, 0, };
		s_Data.FullscreenQuadIndexBuffer = IndexBuffer::Create(indices, 6);
	}

	void Renderer::Init(const RendererConfiguration& config)
	{
		s_Data.Configuration = config;
	
		s_Data.APIContext = APIContext::Create();
		s_RendererAPI = CreateRendererAPI();	
		s_Data.QueueData.Init(s_Data.Configuration.FramesInFlight);	
	}

	void Renderer::InitAPI(bool initDefaultResources)
	{
		s_RendererAPI->Init();
		SetupFullscreenQuad();
		if (initDefaultResources)
			s_Data.Resources.Init();

		WaitAndRenderAll();
	}


	void Renderer::Shutdown()
	{	
		s_Data.FullscreenQuadVertexBuffer.Reset();
		s_Data.FullscreenQuadIndexBuffer.Reset();
		s_Data.ShaderDependencies.Clear();
		s_Data.Resources.Shutdown();
		s_RendererAPI->Shutdown();
		s_Data.QueueData.Shutdown();

		delete s_RendererAPI;
		s_RendererAPI = nullptr;

		s_Data.APIContext->Shutdown(); // Free context to make sure it is destroyed sooner than Logger;
	}

	

	void Renderer::BeginFrame()
	{
		s_RendererAPI->BeginFrame();
	}

	void Renderer::EndFrame()
	{
		s_RendererAPI->EndFrame();
	}

	void Renderer::BeginRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer, const Ref<RenderPass>& renderPass,
		bool subPass, bool clear)
	{
		XYZ_ASSERT(renderPass.Raw(), "Render pass can not be null");
		s_RendererAPI->BeginRenderPass(renderCommandBuffer, renderPass, subPass, clear);
	}

	void Renderer::EndRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer)
	{
		s_RendererAPI->EndRenderPass(renderCommandBuffer);
	}

	void Renderer::RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline,
		Ref<MaterialInstance> material, Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const PushConstBuffer& constData, uint32_t indexCount, uint32_t vertexOffsetSize)
	{
		s_RendererAPI->RenderGeometry(renderCommandBuffer, pipeline, material, vertexBuffer, indexBuffer, constData, indexCount, vertexOffsetSize);
	}

	void Renderer::RenderMesh(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material, Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const PushConstBuffer& constData)
	{
		s_RendererAPI->RenderMesh(renderCommandBuffer, pipeline, material, vertexBuffer, indexBuffer, constData);
	}



	void Renderer::RenderMesh(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material, Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const PushConstBuffer& constData, Ref<VertexBufferSet> instanceBuffer, uint32_t instanceOffset, uint32_t instanceCount)
	{
		s_RendererAPI->RenderMesh(renderCommandBuffer, pipeline, material, vertexBuffer, indexBuffer, constData, instanceBuffer, instanceOffset, instanceCount);
	}

	void Renderer::RenderMesh(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material, Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const PushConstBuffer& constData, Ref<StorageBufferSet> instanceBuffer, uint32_t set, uint32_t binding, uint32_t instanceOffset, uint32_t instanceCount)
	{
		s_RendererAPI->RenderMesh(renderCommandBuffer, pipeline, material, vertexBuffer, indexBuffer, constData, instanceBuffer, set, binding, instanceOffset, instanceCount);
	}

	void Renderer::RenderMesh(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material, Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, Ref<VertexBufferSet> transformBuffer, uint32_t transformOffset, uint32_t transformInstanceCount, Ref<VertexBufferSet> instanceBuffer, uint32_t instanceOffset, uint32_t instanceCount)
	{
		s_RendererAPI->RenderMesh(renderCommandBuffer, pipeline, material, vertexBuffer, indexBuffer, transformBuffer, transformOffset, transformInstanceCount, instanceBuffer, instanceOffset, instanceCount);
	}

	void Renderer::RenderIndirectMesh(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material, Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const PushConstBuffer& constData, Ref<StorageBufferSet> indirectBuffer, uint32_t indirectOffset, uint32_t indirectCount, uint32_t indirectStride)
	{
		s_RendererAPI->RenderIndirect(
			renderCommandBuffer, pipeline, material,
			vertexBuffer, indexBuffer, constData,
			indirectBuffer, indirectOffset, indirectCount, indirectStride
		);
	}

	void Renderer::SubmitFullscreenQuad(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material)
	{
		s_RendererAPI->RenderGeometry(renderCommandBuffer, pipeline, material, s_Data.FullscreenQuadVertexBuffer, s_Data.FullscreenQuadIndexBuffer);
	}

	void Renderer::SubmitFullscreenQuad(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<MaterialInstance> material, const PushConstBuffer& constData)
	{
		s_RendererAPI->RenderGeometry(renderCommandBuffer, pipeline, material, s_Data.FullscreenQuadVertexBuffer, s_Data.FullscreenQuadIndexBuffer, constData);
	}

	void Renderer::BindPipeline(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<UniformBufferSet> uniformBufferSet, Ref<StorageBufferSet> storageBufferSet, Ref<Material> material)
	{
		s_RendererAPI->BindPipeline(renderCommandBuffer, pipeline, uniformBufferSet, storageBufferSet, material);
	}


	void Renderer::BeginPipelineCompute(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<PipelineCompute> pipeline, Ref<UniformBufferSet> uniformBufferSet, Ref<StorageBufferSet> storageBufferSet, Ref<Material> material)
	{
		s_RendererAPI->BeginPipelineCompute(renderCommandBuffer, pipeline, uniformBufferSet, storageBufferSet, material);
	}


	void Renderer::DispatchCompute(Ref<PipelineCompute> pipeline, Ref<MaterialInstance> material, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ, const PushConstBuffer& constData)
	{
		s_RendererAPI->DispatchCompute(pipeline, material, groupCountX, groupCountY, groupCountZ, constData);
	}

	void Renderer::EndPipelineCompute(Ref<PipelineCompute> pipeline)
	{
		s_RendererAPI->EndPipelineCompute(pipeline);
	}

	void Renderer::UpdateDescriptors(Ref<PipelineCompute> pipeline, Ref<Material> material, Ref<UniformBufferSet> uniformBufferSet, Ref<StorageBufferSet> storageBufferSet)
	{
		s_RendererAPI->UpdateDescriptors(pipeline, material, uniformBufferSet, storageBufferSet);
	}

	void Renderer::UpdateDescriptors(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<Material> material, Ref<UniformBufferSet> uniformBufferSet, Ref<StorageBufferSet> storageBufferSet)
	{
		s_RendererAPI->UpdateDescriptors(renderCommandBuffer, pipeline, material, uniformBufferSet, storageBufferSet);
	}

	void Renderer::ClearImage(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Image2D> image)
	{
		s_RendererAPI->ClearImage(renderCommandBuffer, image);
	}

	void Renderer::RegisterShaderDependency(const Ref<Shader>& shader, const Ref<PipelineCompute>& pipeline)
	{
		s_Data.ShaderDependencies.Register(shader->GetHash(), pipeline);
	}
	void Renderer::RegisterShaderDependency(const Ref<Shader>& shader, const Ref<Pipeline>& pipeline)
	{
		s_Data.ShaderDependencies.Register(shader->GetHash(), pipeline);
	}
	void Renderer::RegisterShaderDependency(const Ref<Shader>& shader, const Ref<Material>& material)
	{
		s_Data.ShaderDependencies.Register(shader->GetHash(), material);
	}
	
	
	void Renderer::RemoveShaderDependency(size_t hash)
	{
		s_Data.ShaderDependencies.RemoveDependency(hash);
	}
	void Renderer::OnShaderReload(size_t hash)
	{
		s_Data.ShaderDependencies.OnReload(hash);
	}
	void Renderer::BlockRenderThread()
	{
		s_Data.QueueData.BlockRenderThread();
	}

	void Renderer::WaitAndRenderAll()
	{
		BlockRenderThread();
		Render();
		BlockRenderThread();
		Render();
		BlockRenderThread();
	}

	void Renderer::WaitAndRender()
	{
		Render();
		BlockRenderThread();
	}

	ThreadPool& Renderer::GetPool()
	{
		return s_Data.QueueData.GetThreadPool();
	}

	const RendererStats& Renderer::GetStats()
	{
		return s_Data.Stats;
	}

	uint32_t Renderer::GetCurrentFrame()
	{
		return s_Data.APIContext->GetCurrentFrame();
	}

	void Renderer::Render()
	{
		s_Data.Stats.Reset();
		s_Data.QueueData.ExecuteRenderQueue();
	}
	void Renderer::ExecuteResources()
	{
		uint32_t currentFrame = s_Data.APIContext->GetCurrentFrame();
		s_Data.QueueData.ExecuteResourceQueue(currentFrame);
	}

	void Renderer::ExecuteResources(uint32_t index)
	{
		s_Data.QueueData.ExecuteResourceQueue(index);
	}

	Ref<APIContext> Renderer::GetAPIContext()
	{
		return s_Data.APIContext;
	}
	const RendererResources& Renderer::GetDefaultResources()
	{
		return s_Data.Resources;
	}

	const RenderAPICapabilities& Renderer::GetCapabilities()
	{
		return s_RendererAPI->GetCapabilities();
	}

	const RendererConfiguration& Renderer::GetConfiguration()
	{
		return s_Data.Configuration;
	}

	ScopedLock<RenderCommandQueue> Renderer::getResourceQueue()
	{
		return s_Data.QueueData.GetResourceQueue(s_Data.APIContext->GetCurrentFrame());
	}

	ScopedLock<RenderCommandQueue> Renderer::getRenderCommandQueue()
	{
		return s_Data.QueueData.GetRenderCommandQueue();
	}
	RendererStats& Renderer::getStats()
	{
		return s_Data.Stats;
	}
	RendererStats::RendererStats()
		:
		DrawArraysCount(0), DrawIndexedCount(0), DrawInstancedCount(0), DrawFullscreenCount(0), DrawIndirectCount(0), CommandsCount(0)
	{
	}
	void RendererStats::Reset()
	{
		DrawArraysCount = 0;
		DrawIndexedCount = 0;
		DrawInstancedCount = 0;
		DrawFullscreenCount = 0;
		DrawIndirectCount = 0;
		CommandsCount = 0;
	}
	void RendererResources::Init()
	{
		Includer.AddIncludes("Resources/Shaders/Includes");

		auto whiteTexture = AssetManager::GetAsset<Texture2D>("Resources/Textures/WhiteTexture.tex");
		whiteTexture->SetFlag(AssetFlag::ReadOnly);
		RendererAssets["WhiteTexture"] = whiteTexture;
		
		auto defaultQuadMaterial				= AssetManager::GetAssetWait<MaterialAsset>("Resources/Materials/DefaultLit.mat");
		auto defaultLineMaterial				= AssetManager::GetAssetWait<MaterialAsset>("Resources/Materials/DefaultLine.mat");
		auto defaultCircleMaterial				= AssetManager::GetAssetWait<MaterialAsset>("Resources/Materials/DefaultCircle.mat");
		auto overlayQuadMaterial				= AssetManager::GetAssetWait<MaterialAsset>("Resources/Materials/OverlayQuad.mat");
		auto overlayLineMaterial				= AssetManager::GetAssetWait<MaterialAsset>("Resources/Materials/OverlayLine.mat");
		auto overlayCircleMaterial				= AssetManager::GetAssetWait<MaterialAsset>("Resources/Materials/OverlayCircle.mat");
		auto defaultParticleMaterial			= AssetManager::GetAssetWait<MaterialAsset>("Resources/Materials/DefaultParticle.mat");
		auto defaultDepth3DMaterial				= AssetManager::GetAssetWait<MaterialAsset>("Resources/Materials/PreDepth.mat");
		auto defaultDepth3DAnimMaterial			= AssetManager::GetAssetWait<MaterialAsset>("Resources/Materials/PreDepthAnim.mat");
		auto defaultDepth2DMaterial				= AssetManager::GetAssetWait<MaterialAsset>("Resources/Materials/PreDepth2D.mat");
		auto defaultDepthInstancedMaterial		= AssetManager::GetAssetWait<MaterialAsset>("Resources/Materials/PreDepthInstanced.mat");
		auto lightCullingMaterial				= AssetManager::GetAssetWait<MaterialAsset>("Resources/Materials/LightCulling.mat");
		auto gridMaterial						= AssetManager::GetAssetWait<MaterialAsset>("Resources/Materials/Grid.mat");
		auto animationPBRMaterial				= AssetManager::GetAssetWait<MaterialAsset>("Resources/Materials/AnimationPBR.mat");
		
		
		

		defaultQuadMaterial->SetFlag(AssetFlag::ReadOnly);
		RendererAssets["QuadMaterial"] = defaultQuadMaterial;

		defaultLineMaterial->SetFlag(AssetFlag::ReadOnly);
		RendererAssets["LineMaterial"] = defaultLineMaterial;

		defaultCircleMaterial->SetFlag(AssetFlag::ReadOnly);
		RendererAssets["CircleMaterial"] = defaultCircleMaterial;

		overlayQuadMaterial->SetFlag(AssetFlag::ReadOnly);
		RendererAssets["OverlayQuadMaterial"] = overlayQuadMaterial;

		overlayLineMaterial->SetFlag(AssetFlag::ReadOnly);
		RendererAssets["OverlayLineMaterial"] = overlayLineMaterial;

		overlayCircleMaterial->SetFlag(AssetFlag::ReadOnly);
		RendererAssets["OverlayCircleMaterial"] = overlayCircleMaterial;


		defaultParticleMaterial->SetFlag(AssetFlag::ReadOnly);
		RendererAssets["ParticleMaterial"] = defaultParticleMaterial;


		defaultDepth3DMaterial->SetFlag(AssetFlag::ReadOnly);
		RendererAssets["Depth3DMaterial"] = defaultDepth3DMaterial;


		defaultDepth3DAnimMaterial->SetFlag(AssetFlag::ReadOnly);
		RendererAssets["Depth3DMaterialAnim"] = defaultDepth3DAnimMaterial;


		defaultDepth2DMaterial->SetFlag(AssetFlag::ReadOnly);
		RendererAssets["Depth2DMaterial"] = defaultDepth2DMaterial;


		defaultDepthInstancedMaterial->SetFlag(AssetFlag::ReadOnly);
		RendererAssets["DepthInstancedMaterial"] = defaultDepthInstancedMaterial;


		lightCullingMaterial->SetFlag(AssetFlag::ReadOnly);
		RendererAssets["LightCullingMaterial"] = lightCullingMaterial;


		gridMaterial->SetFlag(AssetFlag::ReadOnly);
		RendererAssets["GridMaterial"] = gridMaterial;


		animationPBRMaterial->SetFlag(AssetFlag::ReadOnly);
		RendererAssets["AnimationPBR"] = animationPBRMaterial;
	}
	void RendererResources::Shutdown()
	{
		for (auto asset : RendererAssets)
			asset.second.Reset();

		RendererAssets.clear();
	}

}