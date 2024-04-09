#include "stdafx.h"
#include "VoxelRenderer.h"

#include "Renderer.h"

#include "XYZ/API/Vulkan/VulkanPipelineCompute.h"
#include "XYZ/API/Vulkan/VulkanStorageBufferSet.h"

#include "XYZ/Utils/Math/Math.h"
#include "XYZ/Utils/Math/AABB.h"
#include "XYZ/Utils/Random.h"

#include "XYZ/ImGui/ImGui.h"


#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace XYZ {

#define TILE_SIZE 16

	
	static AABB VoxelModelToAABB(const glm::mat4& transform, uint32_t width, uint32_t height, uint32_t depth, float voxelSize)
	{
		AABB result;
		result.Min = glm::vec3(0.0f);
		result.Max = glm::vec3(width, height, depth) * voxelSize;

		result = result.TransformAABB(transform);
		return result;
	}


	VoxelRenderer::VoxelRenderer()
	{
		m_CommandBuffer = PrimaryRenderCommandBuffer::Create(0, "VoxelRenderer");
		m_CommandBuffer->CreateTimestampQueries(GPUTimeQueries::Count());

		const uint32_t framesInFlight = Renderer::GetConfiguration().FramesInFlight;
		m_UniformBufferSet = UniformBufferSet::Create(framesInFlight);
		m_UniformBufferSet->Create(sizeof(UBVoxelScene), UBVoxelScene::Set, UBVoxelScene::Binding);
		uint32_t size = sizeof(UBVoxelScene);
		m_StorageBufferSet = StorageBufferSet::Create(framesInFlight);
		m_StorageBufferSet->Create(SSBOVoxels::MaxVoxels, SSBOVoxels::Set, SSBOVoxels::Binding, false, true);
		m_StorageBufferSet->Create(sizeof(SSBOVoxelModels), SSBOVoxelModels::Set, SSBOVoxelModels::Binding);
		m_StorageBufferSet->Create(SSBOColors::MaxSize, SSBOColors::Set, SSBOColors::Binding, false, true);
		m_StorageBufferSet->Create(sizeof(SSBOVoxelCompressed), SSBOVoxelCompressed::Set, SSBOVoxelCompressed::Binding, false, true);
		m_StorageBufferSet->Create(SSBOVoxelComputeData::MaxSize, SSBOVoxelComputeData::Set, SSBOVoxelComputeData::Binding, false, true);
		m_StorageBufferSet->Create(sizeof(SSBOBVH), SSBOBVH::Set, SSBOBVH::Binding);
		m_StorageBufferSet->Create(sizeof(SSBOModelGrid), SSBOModelGrid::Set, SSBOModelGrid::Binding);

		m_VoxelStorageAllocator = Ref<StorageBufferAllocator>::Create(SSBOVoxels::MaxVoxels, SSBOVoxels::Binding, SSBOVoxels::Set);
		m_ColorStorageAllocator = Ref<StorageBufferAllocator>::Create(SSBOColors::MaxSize, SSBOColors::Binding, SSBOColors::Set);
		m_CompressedCellAllocator = Ref<StorageBufferAllocator>::Create(sizeof(SSBOVoxelCompressed), SSBOVoxelCompressed::Binding, SSBOVoxelCompressed::Set);
		m_ComputeStorageAllocator = Ref<StorageBufferAllocator>::Create(SSBOVoxelComputeData::MaxSize, SSBOVoxelComputeData::Binding, SSBOVoxelComputeData::Set);

		TextureProperties props;
		props.Storage = true;
		props.SamplerWrap = TextureWrap::Clamp;
		m_OutputTexture = Texture2D::Create(ImageFormat::RGBA32F, 1280, 720, nullptr, props);
		m_NormalTexture = Texture2D::Create(ImageFormat::RGBA32F, 1280, 720, nullptr, props);
		m_PositionTexture = Texture2D::Create(ImageFormat::RGBA32F, 1280, 720, nullptr, props);
		m_DepthTexture = Texture2D::Create(ImageFormat::RED32F, 1280, 720, nullptr, props);
		m_SSGITexture = Texture2D::Create(ImageFormat::RGBA16F, 1280, 720, nullptr, props);
		createDefaultPipelines();


		m_UBVoxelScene.DirectionalLight.Direction = { -0.2f, -1.4f, -1.5f };
		m_UBVoxelScene.DirectionalLight.Radiance = glm::vec3(1.0f);
		m_UBVoxelScene.DirectionalLight.Multiplier = 1.0f;
		m_WorkGroups = { 32, 32 };

		m_DebugRenderer = std::make_unique<VoxelRendererDebug>(m_CommandBuffer, m_UniformBufferSet);
	}
	void VoxelRenderer::BeginScene(const VoxelRendererCamera& camera)
	{
		m_Frustum = camera.Frustum;
		m_UBVoxelScene.InverseProjection = glm::inverse(camera.Projection);
		m_UBVoxelScene.InverseView = glm::inverse(camera.ViewMatrix);
		m_UBVoxelScene.CameraPosition = glm::vec4(camera.CameraPosition, 1.0f);
		m_UBVoxelScene.ViewportSize.x = m_ViewportSize.x;
		m_UBVoxelScene.ViewportSize.y = m_ViewportSize.y;
		m_SSBOVoxelModels.NumModels = 0;
		
		m_RenderModelsSorted.clear();
		m_RenderModels.clear();
		m_VoxelMeshBuckets.clear();
		m_EffectCommands.clear();

		m_Statistics = {};

		updateViewportSize();
		updateUniformBufferSet();
		m_ModelsBVH.Clear();
	}
	void VoxelRenderer::EndScene()
	{
		prepareModels();

		m_CommandBuffer->Begin();
		m_GPUTimeQueries.GPUTime = m_CommandBuffer->BeginTimestampQuery();

	
		effectPass();
		clearPass();
		renderPass();
		lightPass();
		if (m_UseSSGI)
			ssgiPass();
		if (m_Debug)
			debugPass();

		m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.GPUTime);

		m_CommandBuffer->End();
		m_CommandBuffer->Submit();

		m_LastFrameMeshAllocations = std::move(m_MeshAllocations);
	}
	void VoxelRenderer::SetViewportSize(uint32_t width, uint32_t height)
	{
		if (m_ViewportSize.x != width || m_ViewportSize.y != height)
		{
			m_ViewportSize = glm::ivec2(width, height);
			m_ViewportSizeChanged = true;
		}
		m_DebugRenderer->SetViewportSize(width, height);
	}

	bool VoxelRenderer::SubmitMesh(const Ref<VoxelMesh>& mesh, const glm::mat4& transform)
	{
		bool result = false;
		const auto& submeshes = mesh->GetSubmeshes();
		for (const auto& instance : mesh->GetInstances())
		{
			const glm::mat4 instanceTransform = transform * instance.Transform;
			const VoxelSubmesh& submesh = submeshes[instance.SubmeshIndex];

		
			result |= submitSubmesh(mesh, submesh, instanceTransform, instance.SubmeshIndex);
		}	
		return result;
	}

	bool VoxelRenderer::SubmitMesh(const Ref<VoxelMesh>& mesh, const glm::mat4& transform, const uint32_t* keyFrames)
	{
		bool result = false;
		const auto& submeshes = mesh->GetSubmeshes();
		uint32_t index = 0;
		for (const auto& instance : mesh->GetInstances())
		{
			const uint32_t submeshIndex = instance.ModelAnimation.SubmeshIndices[keyFrames[index]];
			const VoxelSubmesh& submesh = submeshes[submeshIndex];
			const glm::mat4 instanceTransform = transform * instance.Transform;

			result |= submitSubmesh(mesh, submesh, instanceTransform, submeshIndex);
			index++;
		}	
		return result;
	}

	Ref<Image2D> VoxelRenderer::GetFinalPassImage() const
	{
		if (m_Debug)
			return m_DebugRenderer->GetFinalImage();
		if (m_UseSSGI)
			return m_SSGITexture->GetImage();
		return m_OutputTexture->GetImage();
	}

	bool VoxelRenderer::CreateComputeAllocation(uint32_t size, StorageBufferAllocation& allocation)
	{
		return m_ComputeStorageAllocator->Allocate(size, allocation);
	}

	void VoxelRenderer::SubmitComputeData(const void* data, uint32_t size, uint32_t offset, const StorageBufferAllocation& allocation, bool allFrames)
	{
		XYZ_ASSERT(offset + size <= allocation.GetSize(), "");
		if (allFrames)
			m_StorageBufferSet->UpdateEachFrame(data, size, allocation.GetOffset() + offset, allocation.GetBinding(), allocation.GetSet());
		else
			m_StorageBufferSet->Update(data, size, allocation.GetOffset() + offset, allocation.GetBinding(), allocation.GetSet());
	}

	bool VoxelRenderer::IsMeshAllocated(const Ref<VoxelMesh>& mesh) const
	{
		if (m_LastFrameMeshAllocations.find(mesh->GetHandle()) != m_LastFrameMeshAllocations.end())
			return true;

		if (m_MeshAllocations.find(mesh->GetHandle()) != m_MeshAllocations.end())
			return true;
		
		return false;
	}
	
	void VoxelRenderer::SubmitEffect(const Ref<MaterialAsset>& material, const glm::ivec3& workGroups, const PushConstBuffer& constants)
	{
		auto& effectCommand = m_EffectCommands[material->GetHandle()];
		effectCommand.Material = material;
		auto& invocation = effectCommand.Invocations.emplace_back();
		invocation.WorkGroups = workGroups;
		invocation.Constants = constants;
	}

	void VoxelRenderer::OnImGuiRender()
	{
		if (ImGui::Begin("Voxel Renderer"))
		{
			if (ImGui::Button("Reload Shader"))
			{
				Ref<Shader> shader = Shader::Create("Resources/Shaders/Voxel/RaymarchShader.glsl");
				if (shader->IsCompiled())
				{
					m_RaymarchMaterial = Material::Create(shader);

					m_RaymarchMaterial->SetImage("o_Image", m_OutputTexture->GetImage());
					m_RaymarchMaterial->SetImage("o_DepthImage", m_DepthTexture->GetImage());
					m_RaymarchMaterial->SetImage("o_Normal", m_NormalTexture->GetImage());
					m_RaymarchMaterial->SetImage("o_Position", m_PositionTexture->GetImage());
					PipelineComputeSpecification spec;
					spec.Shader = shader;

					m_RaymarchPipeline = PipelineCompute::Create(spec);
				}
				else
				{
					XYZ_CORE_WARN("Failed to compile raymarch shader");
				}
			}
			
			if (ImGui::Button("Reload Shader Light"))
			{
				Ref<Shader> shader = Shader::Create("Resources/Shaders/Voxel/VoxelLightShader.glsl");
				if (shader->IsCompiled())
				{
					m_LightMaterial = Material::Create(shader);

					m_LightMaterial->SetImage("o_Image", m_OutputTexture->GetImage());
					m_LightMaterial->SetImage("o_Normal", m_NormalTexture->GetImage());
					m_LightMaterial->SetImage("o_Position", m_PositionTexture->GetImage());
					PipelineComputeSpecification spec;
					spec.Shader = shader;

					m_LightPipeline = PipelineCompute::Create(spec);
				}
				else
				{
					XYZ_CORE_WARN("Failed to compile raymarch shader");
				}
			}

			if (ImGui::Button("Reload Shader SSGI"))
			{
				Ref<Shader> shader = Shader::Create("Resources/Shaders/Voxel/SSGI.glsl");
				m_SSGIMaterial = Material::Create(shader);

				m_SSGIMaterial->SetImage("u_Image", m_OutputTexture->GetImage());
				m_SSGIMaterial->SetImage("u_DepthImage", m_DepthTexture->GetImage());
				m_SSGIMaterial->SetImage("o_SSGIImage", m_SSGITexture->GetImage());

				PipelineComputeSpecification spec;
				spec.Shader = shader;

				m_SSGIPipeline = PipelineCompute::Create(spec);
			}
			if (ImGui::Button("Clear Allocations"))
			{
				m_LastFrameMeshAllocations.clear();
				m_MeshAllocations.clear();
				m_VoxelStorageAllocator = Ref<StorageBufferAllocator>::Create(SSBOVoxels::MaxVoxels, SSBOVoxels::Binding, SSBOVoxels::Set);
				m_ColorStorageAllocator = Ref<StorageBufferAllocator>::Create(SSBOColors::MaxSize, SSBOColors::Binding, SSBOColors::Set);
				m_CompressedCellAllocator = Ref<StorageBufferAllocator>::Create(sizeof(SSBOVoxelCompressed), SSBOVoxelCompressed::Binding, SSBOVoxelCompressed::Set);
				m_ComputeStorageAllocator = Ref<StorageBufferAllocator>::Create(SSBOVoxelComputeData::MaxSize, SSBOVoxelComputeData::Binding, SSBOVoxelComputeData::Set);

			}

			ImGui::DragFloat3("Light Direction", glm::value_ptr(m_UBVoxelScene.DirectionalLight.Direction), 0.1f);
			ImGui::DragFloat3("Light Color", glm::value_ptr(m_UBVoxelScene.DirectionalLight.Radiance), 0.1f);
			ImGui::DragFloat("Light Multiplier", &m_UBVoxelScene.DirectionalLight.Multiplier, 0.1f);
			
			ImGui::Checkbox("Debug", &m_Debug);
			if (m_Debug)
			{
				ImGui::Checkbox("Update Debug", &m_DebugRenderer->UpdateCamera);
				ImGui::Checkbox("Debug Opaque", &m_DebugOpaque);
				ImGui::InputInt("BVH Depth", &m_ShowBVHDepth);
			}

			ImGui::Checkbox("SSGI", &m_UseSSGI);
			ImGui::DragInt("SSGI Sample Count", (int*) & m_SSGIValues.SampleCount);
			ImGui::DragFloat("SSGI Indirect Amount", &m_SSGIValues.IndirectAmount, 0.1f);
			ImGui::DragFloat("SSGI Noise Amount", &m_SSGIValues.NoiseAmount, 0.1f);
			ImGui::Checkbox("SSGI Noise", (bool*)&m_SSGIValues.Noise);
			ImGui::NewLine();

			ImGui::Checkbox("AABB Grid", &m_UseAABBGrid);
			ImGui::Checkbox("BVH", &m_UseBVH);
			ImGui::Checkbox("Show BVH", &m_ShowBVH);

			ImGui::Checkbox("Show Depth", &m_ShowDepth);
			ImGui::Checkbox("Show Normals", &m_ShowNormals);

			ImGui::NewLine();

			if (ImGui::BeginTable("##Statistics", 2, ImGuiTableFlags_SizingFixedFit))
			{
				const uint32_t voxelBufferUsage = 100.0f * static_cast<float>(m_VoxelStorageAllocator->GetAllocatedSize()) / m_VoxelStorageAllocator->GetSize();
				const uint32_t colorBufferUsage = 100.0f * static_cast<float>(m_ColorStorageAllocator->GetAllocatedSize()) / m_ColorStorageAllocator->GetSize();
				const uint32_t compressBufferUsage = 100.0f * static_cast<float>(m_CompressedCellAllocator->GetAllocatedSize()) / m_CompressedCellAllocator->GetSize();

				UI::TextTableRow("%s", "Mesh Allocations:", "%u", static_cast<uint32_t>(m_LastFrameMeshAllocations.size()));
				UI::TextTableRow("%s", "Model Count:", "%u", m_Statistics.ModelCount);

				UI::TextTableRow("%s", "Voxel Buffer Usage:", "%u%%", voxelBufferUsage);
				UI::TextTableRow("%s", "Color Buffer Usage:", "%u%%", colorBufferUsage);
				UI::TextTableRow("%s", "Compress Buffer Usage:", "%u%%", compressBufferUsage);


				const uint32_t frameIndex = Renderer::GetCurrentFrame();
				float gpuTime = m_CommandBuffer->GetExecutionGPUTime(frameIndex, m_GPUTimeQueries.GPUTime);

				UI::TextTableRow("%s", "GPU Time:", "%.3fms", gpuTime);
				
				ImGui::EndTable();
			}
		}
		ImGui::End();
	}


	
	void VoxelRenderer::updateVoxelModelsSSBO()
	{
		XYZ_PROFILE_FUNC("VoxelRenderer::updateVoxelModelsSSBO");
		// Update ssbos
		const uint32_t voxelModelsUpdateSize =
			sizeof(SSBOVoxelModels::NumModels)
			+ sizeof(SSBOVoxelModels::Padding)
			+ m_SSBOVoxelModels.NumModels * sizeof(VoxelModel);

		m_StorageBufferSet->Update((void*)&m_SSBOVoxelModels, voxelModelsUpdateSize, 0, SSBOVoxelModels::Binding, SSBOVoxelModels::Set);
		
	}

	void VoxelRenderer::updateBVHSSBO()
	{
		XYZ_PROFILE_FUNC("VoxelRenderer::updateBVHSSBO");
		// Update octree
		uint32_t nodeCount = 0;
		const glm::vec3 cameraPosition(glm::vec3(m_UBVoxelScene.CameraPosition));
		for (auto& bvhNode : m_ModelsBVH.GetNodes())
		{
			auto& node = m_SSBOBVH.Nodes[nodeCount];
			node.Max = glm::vec4(bvhNode.AABB.Max, 1.0f);
			node.Min = glm::vec4(bvhNode.AABB.Min, 1.0f);
			node.Data = bvhNode.Data;
			node.Depth = bvhNode.Depth;
			node.Left = bvhNode.Left;
			node.Right = bvhNode.Right;
			nodeCount++;
		}
		m_SSBOBVH.NodeCount = nodeCount;

		const uint32_t firstUpdateSize =
			sizeof(SSBOBVH::NodeCount)
			+ sizeof(SSBOBVH::Padding)
			+ sizeof(VoxelModelBVHNode) * nodeCount;

		m_StorageBufferSet->Update(&m_SSBOBVH, firstUpdateSize, 0, SSBOBVH::Binding, SSBOBVH::Set);
	}

	void VoxelRenderer::updateModelGridSSBO()
	{
		XYZ_PROFILE_FUNC("VoxelRenderer::updateModelGridSSBO");
		
		uint32_t cellCount = 0;
		uint32_t modelCount = 0;
		m_SSBOModelGrid.CellSize = m_ModelsGrid.GetCellSize();
		m_SSBOModelGrid.InverseTransform = glm::inverse(glm::translate(glm::mat4(1.0f), m_ModelsGrid.GetPosition()));

		for (const auto& modelCell : m_ModelsGrid.GetCells())
		{
			auto& cell = m_SSBOModelGrid.Cells[cellCount];
			cell.ModelOffset = modelCount;
			cell.ModelCount = static_cast<uint32_t>(modelCell.size());
			for (const int32_t modelIndex : modelCell)
			{
				m_SSBOModelGrid.ModelIndices[modelCount] = static_cast<uint16_t>(modelIndex);
				modelCount++;
			}

			cellCount++;
		}

		const uint32_t firstUpdateSize =
				sizeof(SSBOModelGrid::Dimensions)
			+	sizeof(SSBOModelGrid::PaddingDim)
			+	sizeof(SSBOModelGrid::CellSize)
			+	sizeof(SSBOModelGrid::PaddingSize)
			+	sizeof(SSBOModelGrid::InverseTransform)
			+	sizeof(SSBOGridCell) * cellCount;

		const uint32_t secondUpdateSize = sizeof(SSBOModelGrid::ModelIndices[0]) * modelCount;
		const uint32_t secondUpdateOffset =
				sizeof(SSBOModelGrid::Dimensions)
			+	sizeof(SSBOModelGrid::PaddingDim)
			+	sizeof(SSBOModelGrid::CellSize)
			+	sizeof(SSBOModelGrid::PaddingSize)
			+	sizeof(SSBOModelGrid::InverseTransform)
			+	sizeof(SSBOModelGrid::Cells);


		m_StorageBufferSet->Update(&m_SSBOModelGrid, firstUpdateSize, 0, SSBOModelGrid::Binding, SSBOModelGrid::Set);
		void* secondUpdateData = &reinterpret_cast<uint8_t*>(&m_SSBOModelGrid)[secondUpdateOffset];
		m_StorageBufferSet->Update(secondUpdateData, secondUpdateSize, secondUpdateOffset, SSBOModelGrid::Binding, SSBOModelGrid::Set);
	}

	bool VoxelRenderer::submitSubmesh(const Ref<VoxelMesh>& mesh, const VoxelSubmesh& submesh, const glm::mat4& transform, uint32_t submeshIndex)
	{		
		const uint32_t voxelCount = static_cast<uint32_t>(submesh.ColorIndices.size());

		VoxelRenderModel renderModel;
		renderModel.BoundingBox =  VoxelModelToAABB(transform, submesh.Width, submesh.Height, submesh.Depth, submesh.VoxelSize);
		if (renderModel.BoundingBox.InsideFrustum(m_Frustum))
		{
			renderModel.SubmeshIndex = submeshIndex;
			renderModel.Transform = transform;
			renderModel.Mesh = mesh;
			m_RenderModels.push_back(renderModel);
			m_Statistics.ModelCount++;
			return true;
		}
		return false;
	}

	void VoxelRenderer::clearPass()
	{		
		Renderer::BeginPipelineCompute(
			m_CommandBuffer,
			m_ClearPipeline,
			m_UniformBufferSet,
			m_StorageBufferSet,
			m_ClearMaterial
		);
		Renderer::DispatchCompute(
			m_ClearPipeline,
			nullptr,
			m_WorkGroups.x, m_WorkGroups.y, 1,
			PushConstBuffer
			{
				glm::vec4(0.0, 0.0, 0.0, 0.0),
				std::numeric_limits<float>::max()
			}
		);
		imageBarrier(m_ClearPipeline, m_OutputTexture->GetImage());
		imageBarrier(m_ClearPipeline, m_NormalTexture->GetImage());
		imageBarrier(m_ClearPipeline, m_PositionTexture->GetImage());
		imageBarrier(m_ClearPipeline, m_DepthTexture->GetImage());
		Renderer::EndPipelineCompute(m_ClearPipeline);
	}

	void VoxelRenderer::lightPass()
	{
		Renderer::BeginPipelineCompute(
			m_CommandBuffer,
			m_LightPipeline,
			m_UniformBufferSet,
			m_StorageBufferSet,
			m_LightMaterial
		);
		Renderer::DispatchCompute(
			m_LightPipeline,
			nullptr,
			m_WorkGroups.x, m_WorkGroups.y, 1
		);
		imageBarrier(m_LightPipeline, m_OutputTexture->GetImage());
		imageBarrier(m_LightPipeline, m_NormalTexture->GetImage());
		imageBarrier(m_LightPipeline, m_PositionTexture->GetImage());
		Renderer::EndPipelineCompute(m_LightPipeline);
	}

	void VoxelRenderer::effectPass()
	{
		auto ssboBarrier = [](Ref<VulkanPipelineCompute> pipeline, Ref<VulkanStorageBufferSet> storageBufferSet) {

			Renderer::Submit([pipeline, storageBufferSet]() {
				uint32_t frameIndex = Renderer::GetCurrentFrame();
				auto storageBuffer = storageBufferSet->Get(SSBOVoxels::Binding, SSBOVoxels::Set, frameIndex).As<VulkanStorageBuffer>();
				VkBufferMemoryBarrier bufferBarrier = {};
				bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
				bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT; // Access mask for the source stage
				bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;  // Access mask for the destination stage
				bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				bufferBarrier.buffer = storageBuffer->GetVulkanBuffer();  // The SSBO buffer
				bufferBarrier.offset = 0;                // Offset in the buffer
				bufferBarrier.size = VK_WHOLE_SIZE;      // Size of the buffer
				vkCmdPipelineBarrier(
					pipeline->GetActiveCommandBuffer(),                   // The command buffer
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // Source pipeline stage
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // Destination pipeline stage
					0,                               // Dependency flags
					0, nullptr,                      // Memory barriers (global memory barriers)
					1, &bufferBarrier,               // Buffer memory barriers
					0, nullptr                       // Image memory barriers
				);
				});
		};


		for (auto& [key, effect] : m_EffectCommands)
		{
			Ref<PipelineCompute> pipeline = getEffectPipeline(effect.Material);
			Renderer::BeginPipelineCompute(
				m_CommandBuffer,
				pipeline,
				m_UniformBufferSet,
				m_StorageBufferSet,
				effect.Material->GetMaterial()
			);

			for (auto& invoc : effect.Invocations)
			{
				Renderer::DispatchCompute(
					pipeline,
					nullptr,
					invoc.WorkGroups.x, invoc.WorkGroups.y, invoc.WorkGroups.z,
					invoc.Constants
				);
				ssboBarrier(pipeline, m_StorageBufferSet);
			}

			Renderer::EndPipelineCompute(pipeline);
		}
	}



	void VoxelRenderer::renderPass()
	{
		Renderer::BeginPipelineCompute(
			m_CommandBuffer,
			m_RaymarchPipeline,
			m_UniformBufferSet,
			m_StorageBufferSet,
			m_RaymarchMaterial
		);

		Bool32 useModelGrid = m_UseAABBGrid;
		Bool32 useBVH = m_UseBVH;
		Bool32 showBVH = m_ShowBVH;
		Bool32 showDepth = m_ShowDepth;
		Bool32 showNormals = m_ShowNormals;

		Renderer::DispatchCompute(
			m_RaymarchPipeline,
			nullptr,
			m_WorkGroups.x, m_WorkGroups.y, 1,
			PushConstBuffer{ useModelGrid, useBVH,  showBVH, showDepth, showNormals }
		);


		imageBarrier(m_RaymarchPipeline, m_OutputTexture->GetImage());
		imageBarrier(m_RaymarchPipeline, m_NormalTexture->GetImage());
		imageBarrier(m_RaymarchPipeline, m_PositionTexture->GetImage());
		imageBarrier(m_RaymarchPipeline, m_DepthTexture->GetImage());

		Renderer::EndPipelineCompute(m_RaymarchPipeline);		
	}
	void VoxelRenderer::ssgiPass()
	{
		Renderer::BeginPipelineCompute(
			m_CommandBuffer,
			m_SSGIPipeline,
			m_UniformBufferSet,
			nullptr,
			m_SSGIMaterial
		);
		
		Renderer::DispatchCompute(
			m_SSGIPipeline,
			nullptr,
			m_WorkGroups.x, m_WorkGroups.y, 1,
			PushConstBuffer{ m_SSGIValues }
		);
		
		Renderer::EndPipelineCompute(m_SSGIPipeline);
	}

	void VoxelRenderer::debugPass()
	{
		m_DebugRenderer->BeginScene(
			m_UBVoxelScene.InverseView,
			m_UBVoxelScene.InverseProjection,
			m_UBVoxelScene.CameraPosition
		);
		glm::vec2 coords = {
			m_ViewportSize.x / 2,
			m_ViewportSize.y / 2
		};
		
		if (m_UseAABBGrid)
			m_DebugRenderer->ShowAABBGrid(m_ModelsGrid);
		if (m_UseBVH)
			m_DebugRenderer->ShowBVH(m_ModelsBVH, m_ShowBVHDepth);
		//for (const auto& model : m_RenderModels)
		//{
		//	if (m_DebugOpaque)
		//	{
		//		if (model.Mesh->IsOpaque())
		//		{
		//			m_DebugRenderer->RaymarchSubmesh(
		//				model.SubmeshIndex, model.BoundingBox, model.Transform, model.Mesh, coords
		//			);
		//		}
		//	}
		//	else 
		//	{
		//		if (!model.Mesh->IsOpaque())
		//		{
		//			m_DebugRenderer->RaymarchSubmesh(
		//				model.SubmeshIndex, model.BoundingBox, model.Transform, model.Mesh, coords
		//			);
		//		}
		//	}
		//}
		m_DebugRenderer->EndScene(m_OutputTexture->GetImage());
	}

	void VoxelRenderer::imageBarrier(Ref<PipelineCompute> pipeline, Ref<Image2D> image)
	{
		Ref<VulkanPipelineCompute> vulkanPipeline = pipeline.As<VulkanPipelineCompute>();
		Ref<VulkanImage2D> vulkanImage = image.As<VulkanImage2D>();

		Renderer::Submit([vulkanPipeline, vulkanImage]() {
			VkImageMemoryBarrier imageMemoryBarrier = {};
			imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemoryBarrier.image = vulkanImage->GetImageInfo().Image;
			imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, vulkanImage->GetSpecification().Mips, 0, 1 };
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			vkCmdPipelineBarrier(
				vulkanPipeline->GetActiveCommandBuffer(),
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &imageMemoryBarrier);
			});
	}


	void VoxelRenderer::updateViewportSize()
	{
		if (m_ViewportSizeChanged)
		{
			m_ViewportSizeChanged = false;
			TextureProperties props;
			props.Storage = true;
			props.SamplerWrap = TextureWrap::Clamp;
			m_OutputTexture = Texture2D::Create(ImageFormat::RGBA32F, m_ViewportSize.x, m_ViewportSize.y, nullptr, props);
			m_PositionTexture = Texture2D::Create(ImageFormat::RGBA32F, m_ViewportSize.x, m_ViewportSize.y, nullptr, props);
			m_NormalTexture = Texture2D::Create(ImageFormat::RGBA32F, m_ViewportSize.x, m_ViewportSize.y, nullptr, props);
			m_DepthTexture = Texture2D::Create(ImageFormat::RED32F, m_ViewportSize.x, m_ViewportSize.y, nullptr, props);
			m_SSGITexture = Texture2D::Create(ImageFormat::RGBA16F, m_ViewportSize.x, m_ViewportSize.y, nullptr, props);
			
			m_RaymarchMaterial->SetImage("o_Image", m_OutputTexture->GetImage());
			m_RaymarchMaterial->SetImage("o_DepthImage", m_DepthTexture->GetImage());
			m_RaymarchMaterial->SetImage("o_Normal", m_NormalTexture->GetImage());
			m_RaymarchMaterial->SetImage("o_Position", m_PositionTexture->GetImage());

			m_ClearMaterial->SetImage("o_Image", m_OutputTexture->GetImage());
			m_ClearMaterial->SetImage("o_DepthImage", m_DepthTexture->GetImage());
			m_ClearMaterial->SetImage("o_Normal", m_NormalTexture->GetImage());
			m_ClearMaterial->SetImage("o_Position", m_PositionTexture->GetImage());

			m_LightMaterial->SetImage("o_Image", m_OutputTexture->GetImage());
			m_LightMaterial->SetImage("o_Normal", m_NormalTexture->GetImage());
			m_LightMaterial->SetImage("o_Position", m_PositionTexture->GetImage());

			m_SSGIMaterial->SetImage("u_Image", m_OutputTexture->GetImage());
			m_SSGIMaterial->SetImage("u_DepthImage", m_DepthTexture->GetImage());
			m_SSGIMaterial->SetImage("o_SSGIImage", m_SSGITexture->GetImage());

			m_UBVoxelScene.ViewportSize.x = m_ViewportSize.x;
			m_UBVoxelScene.ViewportSize.y = m_ViewportSize.y;

			m_WorkGroups = {
				(m_ViewportSize.x + m_ViewportSize.x % TILE_SIZE) / TILE_SIZE,
				(m_ViewportSize.y + m_ViewportSize.y % TILE_SIZE) / TILE_SIZE
			};
		}
	}
	void VoxelRenderer::updateUniformBufferSet()
	{
		Ref<VoxelRenderer> instance = this;
		Renderer::Submit([instance]() mutable {

			const uint32_t currentFrame = Renderer::GetCurrentFrame();
			instance->m_UniformBufferSet->Get(UBVoxelScene::Binding, UBVoxelScene::Set, currentFrame)->RT_Update(&instance->m_UBVoxelScene, sizeof(UBVoxelScene), 0);

		});
	}
	


	void VoxelRenderer::prepareModels()
	{
		XYZ_PROFILE_FUNC("VoxelRenderer::prepareModels");
		const glm::vec3 cameraPosition(m_UBVoxelScene.CameraPosition);
		{
			XYZ_PROFILE_FUNC("VoxelRenderer::prepareModelsCopy");

			for (auto& renderModel : m_RenderModels)
			{
				renderModel.DistanceFromCamera = renderModel.BoundingBox.Distance(cameraPosition);
				m_RenderModelsSorted.push_back(&renderModel);
			}
		}
	
		// Sort by distance from camera
		{
			XYZ_PROFILE_FUNC("VoxelRenderer::prepareModelsSort");		
			std::sort(m_RenderModelsSorted.begin(), m_RenderModelsSorted.end(), [&](const VoxelRenderModel* a, const VoxelRenderModel* b) {
				
				const VoxelSubmesh& submeshA = a->Mesh->GetSubmeshes()[a->SubmeshIndex];
				const VoxelSubmesh& submeshB = b->Mesh->GetSubmeshes()[b->SubmeshIndex];

				bool isAOpaque = submeshA.IsOpaque;
				bool isBOpaque = submeshB.IsOpaque;

				if (isAOpaque && isBOpaque) 
				{
					// Both are opaque, sort by DistanceFromCamera
					return a->DistanceFromCamera < b->DistanceFromCamera;
				}
				else if (isAOpaque) 
				{
					// Only a is opaque, prioritize a
					return true;
				}
				else if (isBOpaque) 
				{
					// Only b is opaque, prioritize b
					return false;
				}
				else 
				{
					// Both are transparent, sort by DistanceFromCamera inverse
					return a->DistanceFromCamera > b->DistanceFromCamera;
				}
			});
		}
		int32_t modelIndex = 0;
		
		for (auto model : m_RenderModelsSorted)
		{			
			auto& allocation = m_VoxelMeshBuckets[model->Mesh->GetHandle()];
			model->ModelIndex = modelIndex;
			allocation.Mesh = model->Mesh;
			allocation.Models.push_back(model);
			modelIndex++;
		}
		
		if (m_UseBVH)
		{
			std::vector<BVHConstructData> constructData;
			for (auto model : m_RenderModelsSorted)
			{
				BVHConstructData& data = constructData.emplace_back();
				data.AABB = model->BoundingBox;
				data.Data = model->ModelIndex;
			}
			m_ModelsBVH.Construct(constructData);
		}
		if (m_UseAABBGrid)
		{
			AABB sceneAABB(glm::vec3(FLT_MAX), glm::vec3(FLT_MIN));
			for (auto model : m_RenderModelsSorted)
				sceneAABB.Union(model->BoundingBox);

			const glm::vec3 aabbSize = sceneAABB.GetSize();
			const glm::vec3 cellSize = {
				aabbSize.x / m_SSBOModelGrid.Dimensions.x,
				aabbSize.y / m_SSBOModelGrid.Dimensions.y,
				aabbSize.z / m_SSBOModelGrid.Dimensions.z
			};

			m_ModelsGrid.Initialize(sceneAABB.Min, m_SSBOModelGrid.Dimensions.x, m_SSBOModelGrid.Dimensions.y, m_SSBOModelGrid.Dimensions.z, cellSize);
			for (auto model : m_RenderModelsSorted)
				m_ModelsGrid.Insert(model->BoundingBox, model->ModelIndex);
		}
		
		// Pass it to ssbo data
		for (auto& [key, meshAllocation] : m_VoxelMeshBuckets)
		{
			if (meshAllocation.Models.empty())
				continue;

			MeshAllocation& meshAlloc = createMeshAllocation(meshAllocation.Mesh);

			for (const auto cmdModel : meshAllocation.Models)
			{
				const VoxelSubmesh& submesh = cmdModel->Mesh->GetSubmeshes()[cmdModel->SubmeshIndex];
				VoxelModel& model = m_SSBOVoxelModels.Models[cmdModel->ModelIndex];
				model.ColorIndex = meshAlloc.ColorAllocation.GetOffset() / SSBOColors::ColorPalleteSize;
				model.VoxelOffset = meshAlloc.Offsets[cmdModel->SubmeshIndex].Voxel;
				model.InverseTransform = glm::inverse(cmdModel->Transform);
				model.Width = submesh.Width;
				model.Height = submesh.Height;
				model.Depth = submesh.Depth;
				model.VoxelSize = submesh.VoxelSize;
				model.Compressed = false;
				model.DistanceFromCamera = cmdModel->DistanceFromCamera;
				model.Opaque = submesh.IsOpaque;

				if (submesh.Compressed)
				{
					model.Compressed = true;
					model.CompressScale = submesh.CompressScale;
					model.CellsOffset = meshAlloc.Offsets[cmdModel->SubmeshIndex].CompressedCell;
				}
				m_SSBOVoxelModels.NumModels++;
			}

		}
		updateVoxelModelsSSBO();
		if (m_UseBVH)
			updateBVHSSBO();
		if (m_UseAABBGrid)
			updateModelGridSSBO();
	}
	
	void VoxelRenderer::createDefaultPipelines()
	{
		Ref<Shader> shader = Shader::Create("Resources/Shaders/Voxel/RaymarchShader.glsl");
		m_RaymarchMaterial = Material::Create(shader);

		m_RaymarchMaterial->SetImage("o_Image", m_OutputTexture->GetImage());
		m_RaymarchMaterial->SetImage("o_DepthImage", m_DepthTexture->GetImage());
		m_RaymarchMaterial->SetImage("o_Normal", m_NormalTexture->GetImage());
		m_RaymarchMaterial->SetImage("o_Position", m_PositionTexture->GetImage());

		PipelineComputeSpecification spec;
		spec.Shader = shader;
	
		m_RaymarchPipeline = PipelineCompute::Create(spec);


		Ref<Shader> clearShader = Shader::Create("Resources/Shaders/Voxel/ImageClearShader.glsl");
		m_ClearMaterial = Material::Create(clearShader);
		m_ClearMaterial->SetImage("o_Image", m_OutputTexture->GetImage());
		m_ClearMaterial->SetImage("o_DepthImage", m_DepthTexture->GetImage());
		m_ClearMaterial->SetImage("o_Normal", m_NormalTexture->GetImage());
		m_ClearMaterial->SetImage("o_Position", m_PositionTexture->GetImage());

		spec.Shader = clearShader;
		m_ClearPipeline = PipelineCompute::Create(spec);


		Ref<Shader> lightShader = Shader::Create("Resources/Shaders/Voxel/VoxelLightShader.glsl");
		m_LightMaterial = Material::Create(lightShader);
		m_LightMaterial->SetImage("o_Image", m_OutputTexture->GetImage());
		m_LightMaterial->SetImage("o_Normal", m_NormalTexture->GetImage());
		m_LightMaterial->SetImage("o_Position", m_PositionTexture->GetImage());

		spec.Shader = lightShader;
		m_LightPipeline = PipelineCompute::Create(spec);


		Ref<Shader> ssgiShader = Shader::Create("Resources/Shaders/Voxel/SSGI.glsl");
		m_SSGIMaterial = Material::Create(ssgiShader);
		m_SSGIMaterial->SetImage("u_Image", m_OutputTexture->GetImage());
		m_SSGIMaterial->SetImage("u_DepthImage", m_DepthTexture->GetImage());
		m_SSGIMaterial->SetImage("o_SSGIImage", m_SSGITexture->GetImage());

		spec.Shader = ssgiShader;
		m_SSGIPipeline = PipelineCompute::Create(spec);
	}

	Ref<PipelineCompute> VoxelRenderer::getEffectPipeline(const Ref<MaterialAsset>& material)
	{
		AssetHandle handle = material->GetHandle();
		auto it = m_EffectPipelines.find(handle);
		if (it != m_EffectPipelines.end())
			return it->second;

		PipelineComputeSpecification spec;
		spec.Shader = material->GetShader();
		spec.Specialization = material->GetSpecialization();

		Ref<PipelineCompute> result = PipelineCompute::Create(spec);
		m_EffectPipelines[handle] = result;

		return result;
	}

	VoxelRenderer::MeshAllocation& VoxelRenderer::createMeshAllocation(const Ref<VoxelMesh>& mesh)
	{
		XYZ_PROFILE_FUNC("VoxelRenderer::createMeshAllocation");
		const AssetHandle& meshHandle = mesh->GetHandle();
		// Check if we have cached allocation from previous frame
		auto lastFrame = m_LastFrameMeshAllocations.find(meshHandle);
		if (lastFrame != m_LastFrameMeshAllocations.end())
		{
			// Reuse allocation from previous frame
			m_MeshAllocations[meshHandle] = std::move(lastFrame->second);
			m_LastFrameMeshAllocations.erase(lastFrame);
		}

		MeshAllocation& meshAlloc = m_MeshAllocations[meshHandle];
		reallocateVoxels(mesh, meshAlloc);
		return meshAlloc;
	}
	void VoxelRenderer::reallocateVoxels(const Ref<VoxelMesh>& mesh, MeshAllocation& allocation)
	{
		XYZ_PROFILE_FUNC("VoxelRenderer::reallocateVoxels");
		const auto& submeshes = mesh->GetSubmeshes();
	
		const uint32_t meshSize = mesh->GetNumVoxels() * sizeof(uint8_t);
		const uint32_t colorSize = SSBOColors::ColorPalleteSize;
		const uint32_t compressedCellsSize = mesh->GetNumCompressedCells() * sizeof(VoxelCompressedCell);
	
		const uint32_t voxelAllocationFlags = m_VoxelStorageAllocator->Allocate(meshSize, allocation.VoxelAllocation);
		const uint32_t colorAllocationFlags = m_ColorStorageAllocator->Allocate(colorSize, allocation.ColorAllocation);
		const uint32_t cellAllocationFlags = m_CompressedCellAllocator->Allocate(compressedCellsSize, allocation.CompressAllocation);
	
		// We call it here to clear it from mesh even if reallocated 
		auto dirtySubmeshes = mesh->DirtySubmeshes();
		auto dirtyCells = mesh->DirtyCompressedCells();
	
		{	// Recreate offsets per submesh
			uint32_t voxelOffset = allocation.VoxelAllocation.GetOffset();
			uint32_t cellOffset = allocation.CompressAllocation.GetOffset() / sizeof(VoxelCompressedCell);
			allocation.Offsets.clear();
			for (const auto& submesh : submeshes)
			{
				allocation.Offsets.push_back({ voxelOffset, cellOffset });
				cellOffset += static_cast<uint32_t>(submesh.CompressedCells.size());
				voxelOffset += submesh.ColorIndices.size();
			}
		}
		// Update color pallete
		if (IS_SET(colorAllocationFlags, StorageBufferAllocator::Reallocated) || mesh->ColorPalleteDirty())
		{
			m_StorageBufferSet->Update(mesh->GetColorPallete().data(), allocation.ColorAllocation.GetSize(), allocation.ColorAllocation.GetOffset(), SSBOColors::Binding, SSBOColors::Set);
		}
		// Update voxels
		if (IS_SET(voxelAllocationFlags, StorageBufferAllocator::Reallocated))
		{
			for (uint32_t submeshIndex = 0; submeshIndex < submeshes.size(); ++submeshIndex)
			{
				const auto& submesh = submeshes[submeshIndex];
				const uint32_t voxelOffset = allocation.Offsets[submeshIndex].Voxel;
				m_StorageBufferSet->Update(submesh.ColorIndices.data(), submesh.ColorIndices.size(), voxelOffset, SSBOVoxels::Binding, SSBOVoxels::Set);
			}
		}
		else // Update only dirty parts of submeshes
		{			
			for (auto& [submeshIndex, range] : dirtySubmeshes)
			{
				const auto& submesh = submeshes[submeshIndex];
				const uint32_t offset = allocation.Offsets[submeshIndex].Voxel + range.Start; // calculate update offset
				const uint32_t voxelCount = range.End - range.Start; // calculate num voxels to update
	
				// Get voxels pointer
				const uint8_t* updateVoxelData = &submesh.ColorIndices.data()[range.Start];
				m_StorageBufferSet->Update(updateVoxelData, voxelCount, offset, SSBOVoxels::Binding, SSBOVoxels::Set);
			}
		}
		// Update compressed cells
		if (IS_SET(cellAllocationFlags, StorageBufferAllocator::Reallocated))
		{
			for (uint32_t submeshIndex = 0; submeshIndex < submeshes.size(); ++submeshIndex)
			{
				const auto& submesh = submeshes[submeshIndex];
				const uint32_t cellOffset = allocation.Offsets[submeshIndex].CompressedCell * sizeof(VoxelCompressedCell);
				const uint32_t cellsSize = static_cast<uint32_t>(submesh.CompressedCells.size() * sizeof(VoxelCompressedCell));
				m_StorageBufferSet->Update(submesh.CompressedCells.data(), cellsSize, cellOffset, SSBOVoxelCompressed::Binding, SSBOVoxelCompressed::Set);
			}
		}
		else // Update only dirty cells
		{		
			for (auto& [submeshIndex, cells] : dirtyCells)
			{
				const auto& submesh = submeshes[submeshIndex];
	
				for (uint32_t cell : cells)
				{
					const uint32_t cellOffset = (allocation.Offsets[submeshIndex].CompressedCell + cell) * sizeof(VoxelCompressedCell);
					const auto* updateCellData = &submesh.CompressedCells[cell];
					m_StorageBufferSet->Update(updateCellData, sizeof(VoxelCompressedCell), cellOffset, SSBOVoxelCompressed::Binding, SSBOVoxelCompressed::Set);
				}
			}
		}
	
	}
	
}
