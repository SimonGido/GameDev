#include "stdafx.h"
#include "SceneRenderer.h"

#include "Renderer2D.h"
#include "Renderer.h"
#include "MeshFactory.h"


#include "XYZ/Core/Input.h"
#include "XYZ/Debug/Profiler.h"
#include "XYZ/ImGui/ImGui.h"
#include "XYZ/API/Vulkan/VulkanRendererAPI.h"
#include "XYZ/API/Vulkan/VulkanPipelineCompute.h"
#include "XYZ/Scene/SceneEntity.h"
#include "XYZ/Asset/AssetManager.h"

#include "XYZ/Utils/Math/Math.h"

#include <glm/gtx/transform.hpp>

#include <imgui/imgui.h>

namespace XYZ {

	static GeometryRenderQueue::TransformData Mat4ToTransformData(const glm::mat4& transform)
	{
		GeometryRenderQueue::TransformData data;
		data.TransformRow[0] = { transform[0][0], transform[1][0], transform[2][0], transform[3][0] };
		data.TransformRow[1] = { transform[0][1], transform[1][1], transform[2][1], transform[3][1] };
		data.TransformRow[2] = { transform[0][2], transform[1][2], transform[2][2], transform[3][2] };
		return data;
	}

	static void CopyToBoneStorage(GeometryRenderQueue::BoneTransforms& storage, const std::vector<ozz::math::Float4x4>& boneTransforms, const Ref<AnimatedMesh>& mesh)
	{
		if (boneTransforms.empty())
		{
			for (auto& bone : storage)
				bone = ozz::math::Float4x4::identity();
		}
		else
		{
			const auto& boneInfo = mesh->GetMeshSource()->GetBoneInfo();
			for (size_t i = 0; i < boneTransforms.size(); ++i)
			{
				const uint32_t jointIndex = boneInfo[i].JointIndex;
				storage[i] = boneInfo[i].InverseTransform * boneTransforms[jointIndex] * boneInfo[i].BoneOffset;
			}
		}
	}

	SceneRenderer::SceneRenderer(Ref<Scene> scene, SceneRendererSpecification specification)
		:
		m_Specification(specification),
		m_ActiveScene(scene)
	{
		m_ViewportSize = { 1280, 720 };
		XYZ_PROFILE_FUNC("SceneRenderer::SceneRenderer");

		m_CommandBuffer = PrimaryRenderCommandBuffer::Create(0, "SceneRenderer");

		m_CommandBuffer->CreateTimestampQueries(GPUTimeQueries::Count());

		createGeometryPass();
		createCompositePass();
		createLightPass();
		createDepthPass();
		createBloomTextures();
		createGridResources();

		const uint32_t framesInFlight = Renderer::GetConfiguration().FramesInFlight;
		m_UniformBufferSet = UniformBufferSet::Create(framesInFlight);
		m_UniformBufferSet->Create(sizeof(UBCameraData), UBCameraData::Set, UBCameraData::Binding);
		m_UniformBufferSet->Create(sizeof(UBRendererData), UBRendererData::Set, UBRendererData::Binding);
		m_UniformBufferSet->Create(sizeof(UBSceneData), UBSceneData::Set, UBSceneData::Binding);

		m_StorageBufferSet = StorageBufferSet::Create(framesInFlight);
		m_StorageBufferSet->Create(sizeof(SSBOPointLights3D), SSBOPointLights3D::Set, SSBOPointLights3D::Binding);
		m_StorageBufferSet->Create(1, SSBOLightCulling::Set, SSBOLightCulling::Binding);
		m_StorageBufferSet->Create(sizeof(SSBOBoneTransformData), SSBOBoneTransformData::Set, SSBOBoneTransformData::Binding);

		m_StorageBufferSet->Create(SSBOIndirectData::MaxSize, SSBOIndirectData::Set, SSBOIndirectData::Binding, true);

		for (uint32_t i = 0; i < SSBOComputeData::Count; ++i)
		{
			m_StorageBufferSet->Create(SSBOComputeData::MaxSize, SSBOComputeData::Set, SSBOComputeData::Binding[i]);
			m_ComputeDataAllocator[i] = Ref<StorageBufferAllocator>::Create(SSBOComputeData::MaxSize, SSBOComputeData::Binding[i], SSBOComputeData::Set);
		}
		m_IndirectCommandAllocator = Ref<StorageBufferAllocator>::Create(SSBOIndirectData::MaxSize, SSBOIndirectData::Binding, SSBOIndirectData::Set);

		m_CompositeShaderAsset = AssetManager::GetAssetWait<ShaderAsset>("Resources/Shaders/CompositeShader.shader");
		m_LightShaderAsset = AssetManager::GetAssetWait<ShaderAsset>("Resources/Shaders/LightShader.shader");
		m_BloomShaderAsset = AssetManager::GetAssetWait<ShaderAsset>("Resources/Shaders/Bloom.shader");

		m_GeometryPass.Init(this);
		m_DeferredLightPass.Init({ m_LightRenderPass, m_LightShaderAsset->GetShader() }, m_CommandBuffer);
		m_BloomPass.Init({ m_BloomShaderAsset->GetShader(), m_BloomTexture }, m_CommandBuffer);
		m_CompositePass.Init({ m_CompositeRenderPass, m_CompositeShaderAsset->GetShader() }, m_CommandBuffer);
		m_LightCullingPass.Init({ m_UniformBufferSet, m_StorageBufferSet });

		m_LightCullingWorkGroups = {
			(m_ViewportSize.x + m_ViewportSize.x % 16) / 16,
			(m_ViewportSize.y + m_ViewportSize.y % 16) / 16,
			1
		};
	}

	SceneRenderer::~SceneRenderer()
	{		
	}


	void SceneRenderer::SetScene(Ref<Scene> scene)
	{
		m_ActiveScene = scene;
	}

	void SceneRenderer::SetViewportSize(uint32_t width, uint32_t height)
	{
		if (m_ViewportSize.x != width || m_ViewportSize.y != height)
		{
			m_ViewportSize = glm::ivec2(width, height);
			m_ViewportSizeChanged = true;
		}
	}
	void SceneRenderer::BeginScene(const SceneRendererCamera& camera)
	{
		m_SceneCamera = camera;

		m_CameraDataUB.ViewProjectionMatrix = m_SceneCamera.Camera.GetProjectionMatrix() * m_SceneCamera.ViewMatrix;
		m_CameraDataUB.ProjectionMatrix = m_SceneCamera.Camera.GetProjectionMatrix();
		m_CameraDataUB.ViewMatrix = m_SceneCamera.ViewMatrix;
		m_CameraDataUB.CameraPosition = m_SceneCamera.ViewPosition;

		const auto& lightEnvironment = m_ActiveScene->m_LightEnvironment;
		m_SceneDataUB.NumberDirectionalLights = static_cast<uint32_t>(lightEnvironment.DirectionalLights.size());
		memcpy(m_SceneDataUB.DirectionalLights, lightEnvironment.DirectionalLights.data(), lightEnvironment.DirectionalLights.size() * sizeof(DirectionalLight));

		
		m_PointsLights3DSSBO.Count = static_cast<uint32_t>(lightEnvironment.PointLights3D.size());
		memcpy(m_PointsLights3DSSBO.PointLights, lightEnvironment.PointLights3D.data(), lightEnvironment.PointLights3D.size() * sizeof(PointLight3D));
		updateViewportSize();
		updateBufferSets();
	}
	void SceneRenderer::BeginScene(const glm::mat4& viewProjectionMatrix, const glm::mat4& viewMatrix, const glm::mat4& projection)
	{
		m_CameraDataUB.ViewProjectionMatrix = viewProjectionMatrix;
		m_CameraDataUB.ProjectionMatrix = projection;
		m_CameraDataUB.ViewMatrix = viewMatrix;
		m_CameraDataUB.CameraPosition = glm::inverse(viewMatrix)[3];

		const auto& lightEnvironment = m_ActiveScene->m_LightEnvironment;
		m_SceneDataUB.NumberDirectionalLights = static_cast<uint32_t>(lightEnvironment.DirectionalLights.size());
		memcpy(m_SceneDataUB.DirectionalLights, lightEnvironment.DirectionalLights.data(), lightEnvironment.DirectionalLights.size() * sizeof(DirectionalLight));

		m_PointsLights3DSSBO.Count = static_cast<uint32_t>(lightEnvironment.PointLights3D.size());
		memcpy(m_PointsLights3DSSBO.PointLights, lightEnvironment.PointLights3D.data(), lightEnvironment.PointLights3D.size() * sizeof(PointLight3D));
		updateViewportSize();
		updateBufferSets();
	}
	void SceneRenderer::EndScene()
	{
		preRender();

		m_CommandBuffer->Begin();
		m_GPUTimeQueries.GPUTime = m_CommandBuffer->BeginTimestampQuery();

		Ref<Image2D> depthImage = m_DepthRenderPass->GetSpecification().TargetFramebuffer->GetDepthImage();
		Ref<Image2D> colorImage = m_GeometryRenderPass->GetSpecification().TargetFramebuffer->GetImage(0);
		Ref<Image2D> positionImage = m_GeometryRenderPass->GetSpecification().TargetFramebuffer->GetImage(1);
		Ref<Image2D> lightImage = m_LightRenderPass->GetSpecification().TargetFramebuffer->GetImage();
		//depthImage = m_DepthRenderPass->GetSpecification().TargetFramebuffer->GetImage(0);
		

		const bool clearGeometryPass = !m_Options.ShowGrid;
		if (m_Options.ShowGrid)
		{
			Renderer::BeginRenderPass(m_CommandBuffer, m_GeometryRenderPass, false, true);
			renderGrid();
			Renderer::EndRenderPass(m_CommandBuffer);
		}
		m_GeometryPass.PreDepthPass(m_CommandBuffer, m_Queue, m_CameraDataUB.ViewMatrix, true);
		m_GeometryPass.SubmitCompute(m_CommandBuffer, m_Queue);
		m_LightCullingPass.Submit(m_CommandBuffer, depthImage, m_LightCullingWorkGroups, m_ViewportSize);
		m_GeometryPass.Submit(m_CommandBuffer, m_Queue, m_CameraDataUB.ViewMatrix, clearGeometryPass);
		m_DeferredLightPass.Submit(m_CommandBuffer, colorImage, positionImage);
		m_BloomPass.Submit(m_CommandBuffer, colorImage, m_BloomSettings, m_ViewportSize);
		m_CompositePass.Submit(m_CommandBuffer, lightImage, m_BloomTexture[2]->GetImage(), true);


		m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.GPUTime);

		m_CommandBuffer->End();
		m_CommandBuffer->Submit();

		m_Queue.Clear();
		//updateViewportSize();
	}


	void SceneRenderer::SubmitBillboard(const Ref<MaterialAsset>& material, const Ref<SubTexture>& subTexture, uint32_t sortLayer, const glm::vec4& color, const glm::vec3& position, const glm::vec2& size)
	{
		GeometryRenderQueue::SpriteKey key{ material->GetHandle() };

		auto& command = m_Queue.BillboardDrawCommands[key];

		uint32_t textureIndex = command.SetTexture(subTexture->GetTexture());

		command.MaterialAsset = material;
		command.MaterialInstance = material->GetMaterialInstance();

		command.BillboardData.push_back({ textureIndex, subTexture->GetTexCoords(), color, position, size });
	}

	void SceneRenderer::SubmitSprite(const Ref<MaterialAsset>& material, const Ref<SubTexture>& subTexture, const glm::vec4& color, const glm::mat4& transform)
	{
		GeometryRenderQueue::SpriteKey key{ material->GetHandle() };
		auto& command = m_Queue.SpriteDrawCommands[key];
		
		uint32_t textureIndex = command.SetTexture(subTexture->GetTexture());
		
		command.MaterialAsset = material;
		command.MaterialInstance = material->GetMaterialInstance();
		command.SpriteData.push_back({ textureIndex, subTexture->GetTexCoords(), color, transform });
	}

	void SceneRenderer::SubmitMesh(const Ref<Mesh>& mesh, const Ref<MaterialAsset>& material, const glm::mat4& transform, const Ref<MaterialInstance>& overrideMaterial)
	{
		GeometryRenderQueue::BatchMeshKey key{ mesh->GetRenderID(), material->GetHandle() };
		
		auto& dc = m_Queue.MeshDrawCommands[key];
		dc.Mesh = mesh;
		dc.MaterialAsset = material;

		if (overrideMaterial.Raw())
		{
			auto& dcOverride = dc.OverrideCommands.emplace_back();
			dcOverride.OverrideMaterial = overrideMaterial;
			dcOverride.Transform = transform;
		}
		else
		{		
			dc.Count++;
			dc.OverrideMaterial = material->GetMaterialInstance();
			dc.TransformInstanceCount++;
			dc.TransformData.push_back(Mat4ToTransformData(transform));
		}
	}

	void SceneRenderer::SubmitMesh(const Ref<Mesh>& mesh, const Ref<MaterialAsset>& material, const void* instanceData, uint32_t instanceCount, uint32_t instanceSize, const Ref<MaterialInstance>& overrideMaterial)
	{
		GeometryRenderQueue::BatchMeshKey key{ mesh->GetRenderID(), material->GetHandle() };

		auto& dc = m_Queue.InstanceMeshDrawCommands[key];
		dc.Mesh = mesh;
		dc.MaterialAsset = material;
		dc.Transform = glm::mat4(1.0f);

		if (overrideMaterial.Raw())
		{
			XYZ_ASSERT(false, "Not implemented");
		}
		else
		{
			dc.OverrideMaterial = material->GetMaterialInstance();
			dc.InstanceCount += instanceCount;
		
			size_t offset = dc.InstanceData.size();
			size_t instanceDataSize = static_cast<size_t>(instanceCount) * instanceSize;
			dc.InstanceData.resize(offset + instanceDataSize);
			memcpy(dc.InstanceData.data() + offset, instanceData, instanceDataSize);
		}
	}

	void SceneRenderer::SubmitMesh(const Ref<AnimatedMesh>& mesh, const Ref<MaterialAsset>& material, const glm::mat4& transform, const std::vector<ozz::math::Float4x4>& boneTransforms, const Ref<MaterialInstance>& overrideMaterial)
	{
		GeometryRenderQueue::BatchMeshKey key{ mesh->GetHandle(), material->GetHandle() };

		auto& dc = m_Queue.AnimatedMeshDrawCommands[key];
		dc.Mesh = mesh;
		dc.MaterialAsset = material;

		if (overrideMaterial.Raw())
		{
			auto& dcOverride = dc.OverrideCommands.emplace_back();
			dcOverride.OverrideMaterial = overrideMaterial;
			dcOverride.Transform = transform;
			CopyToBoneStorage(dcOverride.BoneTransforms, boneTransforms, mesh);
		}
		else
		{
			dc.Count++;
			dc.OverrideMaterial = material->GetMaterialInstance();
			dc.TransformInstanceCount++;
			dc.TransformData.push_back(Mat4ToTransformData(transform));
			auto& boneStorage = dc.BoneData.emplace_back();
			CopyToBoneStorage(boneStorage, boneTransforms, mesh);
		}
	}



	bool SceneRenderer::CreateComputeAllocation(uint32_t size, uint32_t index, StorageBufferAllocation& allocation)
	{
		XYZ_ASSERT(index <= SSBOComputeData::Count, "");
	
		return m_ComputeDataAllocator.at(index)->Allocate(Math::RoundUp(size, 16), allocation);
	}

	bool SceneRenderer::AllocateIndirectCommand(uint32_t count, StorageBufferAllocation& allocation)
	{
		return m_IndirectCommandAllocator->Allocate(count * sizeof(IndirectIndexedDrawCommand), allocation);
	}


	void SceneRenderer::SubmitComputeData(const void* data, uint32_t size, uint32_t offset, const StorageBufferAllocation& allocation, bool allFrames)
	{
		XYZ_ASSERT(offset + size <= allocation.GetSize(), "");
		if (allFrames)
			m_StorageBufferSet->UpdateEachFrame(data, size, allocation.GetOffset() + offset, allocation.GetBinding(), allocation.GetSet());
		else
			m_StorageBufferSet->Update(data, size, allocation.GetOffset() + offset, allocation.GetBinding(), allocation.GetSet());
	}
	
	void SceneRenderer::SubmitCompute(
		const Ref<MaterialAsset>& materialCompute,
		const ComputeData* computeData, uint32_t count,
		const PushConstBuffer& uniformComputeData
	)
	{
		AssetHandle computeKey = materialCompute->GetHandle();
		auto& computeCommand = m_Queue.ComputeCommands[computeKey];
		computeCommand.MaterialCompute = materialCompute;

		auto& command = computeCommand.ComputeCommands.emplace_back();
		for (uint32_t i = 0; i < count; ++i)
		{
			auto& data = computeData[i];
			auto& allocationData = command.AllocationData.emplace_back();
			const uint32_t dataOffset = command.ComputeData.size();
			
			// Allocation stores info about where is result of computation stored
			allocationData.Allocation = data.Allocation;

			// Store data required for computation
			allocationData.ComputeDataOffset = dataOffset;
			allocationData.ComputeDataSize = data.DataSize;

			command.ComputeData.resize(dataOffset + data.DataSize);
			memcpy(command.ComputeData.data() + dataOffset, data.Data, data.DataSize);
		}
		command.OverrideUniformData = uniformComputeData;
	}


	void SceneRenderer::SubmitMeshIndirect(
		const Ref<Mesh>& mesh,
		const Ref<MaterialAsset>& material,
		const Ref<MaterialInstance>& overrideMaterial,
		const glm::mat4& transform,
		const StorageBufferAllocation& computeReadAllocation,
		const StorageBufferAllocation& indirectCommandAllocation
	)
	{
		AssetHandle renderKey = material->GetHandle();
		auto& dc = m_Queue.IndirectDrawCommands[renderKey];
		dc.MaterialAsset = material;

		auto& renderCommand = dc.OverrideCommands.emplace_back();
		renderCommand.Mesh = mesh;
		renderCommand.ReadStateAllocation = computeReadAllocation;
		renderCommand.IndirectCommandAllocation = indirectCommandAllocation;
		renderCommand.Transform = transform;

		if (overrideMaterial.Raw())
		{
			renderCommand.OverrideMaterial = overrideMaterial;
		}
		else
		{
			renderCommand.OverrideMaterial = material->GetMaterialInstance();
		}
	}


	void SceneRenderer::SetGridProperties(const GridProperties& props)
	{
		m_GridProps = props;
	}

	void SceneRenderer::OnImGuiRender()
	{
		XYZ_PROFILE_FUNC("SceneRenderer::OnImGuiRender");
		
		if (ImGui::Begin("Scene Renderer"))
		{
			if (ImGui::BeginTable("##Viewport", 4, ImGuiTableFlags_SizingFixedFit))
			{
				UI::TextTableRow(
					"%s", "Viewport Width:", "%llu", m_ViewportSize.x,
					"%s", "Viewport Height:", "%llu", m_ViewportSize.y
				);
				ImGui::EndTable();
			}
			if (ImGui::BeginTable("##Specification", 2, ImGuiTableFlags_SizingFixedFit))
			{
				UI::TextTableRow("%s", "Max Point Lights:", "%u", SSBOPointLights3D::MaxLights);
				UI::TextTableRow("%s", "Max Bone Transform:", "%u", SSBOBoneTransformData::MaxBoneTransforms);
				UI::TextTableRow("%s", "Max Indirect Commands:", "%u", SSBOIndirectData::MaxCommands);
				UI::TextTableRow("%s", "Max Compute Data Size:", "%u", SSBOComputeData::MaxSize);

				UI::TextTableRow("%s", "Max Transform Instances:", "%u", GeometryPass::GetTransformBufferCount());
				UI::TextTableRow("%s", "Max Instance Data Size:", "%u", GeometryPass::GetInstanceBufferSize());

				ImGui::EndTable();
			}

			if (UI::BeginTreeNode("Visualization"))
			{
				if (ImGui::BeginTable("##VisualizationTable", 2, ImGuiTableFlags_SizingFixedFit))
				{
					UI::TableRow("ShowLightComplexity",
						[]() { ImGui::Text("Show Light Complexity"); },
						[&]() { ImGui::Checkbox("##ShowLightComplexity", &m_RendererDataUB.ShowLightComplexity); }
					);
					UI::TableRow("ShowGridRow",
						[]() { ImGui::Text("Show Grid"); },
						[&]() { ImGui::Checkbox("##ShowGrid", &m_Options.ShowGrid); }
					);

					UI::TableRow("GridScaleRow",
						[]() { ImGui::Text("Grid Scale"); },
						[&]()
						{	UI::ScopedStyleStack style(true, ImGuiStyleVar_ItemSpacing, ImVec2{ 0.0f, 5.0f });
							if (UI::FloatControl("##GridScale", "##GridScaleDrag", m_GridProps.Scale, 16.025f, 0.05f))
								m_GridMaterialInstance->Set("u_Settings.Scale", m_GridProps.Scale);
						}
					);

					UI::TableRow("LineWidthRow",
						[]() { ImGui::Text("Grid Line Width"); },
						[&]()
						{	UI::ScopedStyleStack style(true, ImGuiStyleVar_ItemSpacing, ImVec2{ 0.0f, 5.0f });
							if (UI::FloatControl("##GridLineWidth", "##GridLineWidthDrag", m_GridProps.LineWidth, 0.025f, 0.05f))
								m_GridMaterialInstance->Set("u_Settings.Size", m_GridProps.LineWidth);
						}
					);
					ImGui::EndTable();
				}
				UI::EndTreeNode();
			}

			if (UI::BeginTreeNode("Buffers Usage"))
			{
				if (ImGui::BeginTable("##BuffersUsage", 2, ImGuiTableFlags_SizingFixedFit))
				{
					const uint32_t transformsBufferUsage	= 100 * m_RenderStatistics.TransformInstanceCount / GeometryPass::GetTransformBufferCount();
					const uint32_t bonesBufferUsage			= 100 * m_GeometryPassStatistics.BoneTransformCount / SSBOBoneTransformData::MaxBoneTransforms;
					const uint32_t instanceBufferUsage		= 100 * m_GeometryPassStatistics.InstanceDataSize / GeometryPass::GetInstanceBufferSize();
					const uint32_t indirectBufferUsage		= 100 * m_IndirectCommandAllocator->GetAllocatedSize() / m_IndirectCommandAllocator->GetSize();
					

					UI::TableRow("",
						[]() { ImGui::Text("Transform Buffer"); },
						[&]() { ImGui::Text("%u%%", transformsBufferUsage); });

					UI::TableRow("",
						[]() { ImGui::Text("Bones Buffer"); },
						[&]() { ImGui::Text("%u%%", bonesBufferUsage); });

					UI::TableRow("",
						[]() { ImGui::Text("Instance Buffer"); },
						[&]() { ImGui::Text("%u%%", instanceBufferUsage); });

					UI::TableRow("",
						[]() { ImGui::Text("Indirect Command Buffer"); },
						[&]() { ImGui::Text("%u%%", indirectBufferUsage); });

					for (uint32_t i = 0; i < SSBOComputeData::Count; ++i)
					{
						const uint32_t computeBufferUsage = 100 * m_ComputeDataAllocator[i]->GetAllocatedSize() / m_ComputeDataAllocator[i]->GetSize();
						UI::TableRow("",
							[i]() { ImGui::Text("Compute Buffer Usage %u", i); },
							[computeBufferUsage]() { ImGui::Text("%u%%", computeBufferUsage); });
					}

					ImGui::EndTable();
				}
				UI::EndTreeNode();
			}


			if (UI::BeginTreeNode("Render Statistics"))
			{
				if (ImGui::BeginTable("##RenderStatistics", 2, ImGuiTableFlags_SizingFixedFit))
				{
					UI::TextTableRow("%s", "Sprite Draw Count:", "%u", m_RenderStatistics.SpriteDrawCommandCount);
					
					UI::TextTableRow("%s", "Mesh Draw Count:", "%u", m_RenderStatistics.MeshDrawCommandCount);
					UI::TextTableRow("%s", "Mesh Override Draw Count:", "%u", m_RenderStatistics.MeshOverrideDrawCommandCount);

					
					UI::TextTableRow("%s", "Animated Mesh Draw Count:", "%u", m_RenderStatistics.AnimatedMeshDrawCommandCount);
					UI::TextTableRow("%s", "Animated Mesh Override Draw Count:", "%u", m_RenderStatistics.AnimatedMeshOverrideDrawCommandCount);


					UI::TextTableRow("%s", "Instance Mesh Draw Count:", "%u", m_RenderStatistics.InstanceMeshDrawCommandCount);


					UI::TextTableRow("%s", "Point Light2D Count:", "%u", m_RenderStatistics.PointLight2DCount);
					UI::TextTableRow("%s", "Spot Light2D Count:", "%u", m_RenderStatistics.SpotLight2DCount);

					UI::TextTableRow("%s", "Transform Instances:", "%u", m_RenderStatistics.TransformInstanceCount);
					UI::TextTableRow("%s", "Instance Data Size:", "%u", m_RenderStatistics.InstanceDataSize);
					
					ImGui::EndTable();
				}
				UI::EndTreeNode();
			}
			const uint32_t frameIndex = Renderer::GetCurrentFrame();
			if (UI::BeginTreeNode("GPU measurements"))
			{
				if (ImGui::BeginTable("##GPUTime", 2, ImGuiTableFlags_SizingFixedFit))
				{
					UI::TextTableRow("%s", "GPU time:", "%.3fms", m_CommandBuffer->GetExecutionGPUTime(frameIndex, m_GPUTimeQueries.GPUTime));
					UI::TextTableRow("%s", "Depth Pass:", "%.3fms", m_CommandBuffer->GetExecutionGPUTime(frameIndex, m_GPUTimeQueries.DepthPassQuery));
					UI::TextTableRow("%s", "Geometry Pass:", "%.3fms", m_CommandBuffer->GetExecutionGPUTime(frameIndex, m_GPUTimeQueries.GeometryPassQuery));
					ImGui::EndTable();
				}
				
				UI::EndTreeNode();
			}
			if (UI::BeginTreeNode("Pipeline Statistics"))
			{
				const PipelineStatistics& pipelineStats = m_CommandBuffer->GetPipelineStatistics(frameIndex);
				if (ImGui::BeginTable("##GPUTime", 2, ImGuiTableFlags_SizingFixedFit))
				{
					UI::TextTableRow("%s", "Input Assembly Vertices:", "%llu", pipelineStats.InputAssemblyVertices);		
					UI::TextTableRow("%s", "Input Assembly Primitives:", "%llu", pipelineStats.InputAssemblyPrimitives);
					UI::TextTableRow("%s", "Vertex Shader Invocations:", "%llu", pipelineStats.VertexShaderInvocations);
					UI::TextTableRow("%s", "Clipping Invocations:", "%llu", pipelineStats.ClippingInvocations);
					UI::TextTableRow("%s", "Clipping Primitives:", "%llu", pipelineStats.ClippingPrimitives);
					UI::TextTableRow("%s", "Fragment Shader Invocations:", "%llu", pipelineStats.FragmentShaderInvocations);
					UI::TextTableRow("%s", "Compute Shader Invocations:", "%llu", pipelineStats.ComputeShaderInvocations);

					ImGui::EndTable();
				}
				UI::EndTreeNode();
			}
			if (UI::BeginTreeNode("Pre Depth Map"))
			{
				// Debug image
				auto image = m_DepthRenderPass->GetSpecification().TargetFramebuffer->GetImage(0);
				const float size = ImGui::GetContentRegionAvail().x;
				UI::Image(image, { size, size });

				UI::EndTreeNode();
			}
		}
		ImGui::End();
	}

	Ref<RenderPass> SceneRenderer::GetFinalRenderPass() const
	{
		return m_CompositeRenderPass;
	}

	Ref<Image2D> SceneRenderer::GetFinalPassImage() const
	{
		return GetFinalRenderPass()->GetSpecification().TargetFramebuffer->GetImage();
	}
	SceneRendererOptions& SceneRenderer::GetOptions()
	{
		return m_Options;
	}

	void SceneRenderer::createCompositePass()
	{
		FramebufferSpecification compFramebufferSpec;
		compFramebufferSpec.ClearColor = { 0.1f, 0.1f, 0.1f, 1.0f };
		compFramebufferSpec.SwapChainTarget = m_Specification.SwapChainTarget;
		compFramebufferSpec.ClearOnLoad = false;
		// No depth for swapchain
		if (m_Specification.SwapChainTarget)
			compFramebufferSpec.Attachments = { ImageFormat::RGBA };
		else
			compFramebufferSpec.Attachments = { ImageFormat::RGBA, ImageFormat::Depth };

		Ref<Framebuffer> framebuffer = Framebuffer::Create(compFramebufferSpec);

		RenderPassSpecification renderPassSpec;
		renderPassSpec.TargetFramebuffer = framebuffer;
		m_CompositeRenderPass = RenderPass::Create(renderPassSpec);
	}
	void SceneRenderer::createLightPass()
	{
		FramebufferSpecification specs;
		specs.ClearColor = { 0.1f,0.1f,0.1f,0.0f };
		specs.Attachments = {
			FramebufferTextureSpecification(ImageFormat::RGBA32F, true)
		};
		Ref<Framebuffer> fbo = Framebuffer::Create(specs);
		m_LightRenderPass = RenderPass::Create({ fbo });
	}

	void SceneRenderer::createGeometryPass()
	{
		FramebufferSpecification framebufferSpec;
		framebufferSpec.Attachments = {
				FramebufferTextureSpecification(ImageFormat::RGBA32F),
				FramebufferTextureSpecification(ImageFormat::RGBA32F),
				FramebufferTextureSpecification(ImageFormat::DEPTH24STENCIL8)
		};
		framebufferSpec.Samples = 1;
		framebufferSpec.ClearOnLoad = false;
		framebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 0.0f };

		Ref<Framebuffer> framebuffer = Framebuffer::Create(framebufferSpec);

		RenderPassSpecification renderPassSpec;
		renderPassSpec.TargetFramebuffer = framebuffer;
		m_GeometryRenderPass = RenderPass::Create(renderPassSpec);
	}
	void SceneRenderer::createDepthPass()
	{
		FramebufferSpecification depthFramebufferSpec;
		depthFramebufferSpec.Attachments = { ImageFormat::RED32F, ImageFormat::DEPTH32F};
		depthFramebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
		

		RenderPassSpecification depthRenderPassSpec;
		depthRenderPassSpec.TargetFramebuffer = Framebuffer::Create(depthFramebufferSpec);

		m_DepthRenderPass = RenderPass::Create(depthRenderPassSpec);
	}
	void SceneRenderer::createBloomTextures()
	{
		TextureProperties props;
		props.Storage = true;
		props.SamplerWrap = TextureWrap::Clamp;

		m_BloomTexture[0] = Texture2D::Create(ImageFormat::RGBA32F, 1280, 720, nullptr, props);
		m_BloomTexture[1] = Texture2D::Create(ImageFormat::RGBA32F, 1280, 720, nullptr, props);
		m_BloomTexture[2] = Texture2D::Create(ImageFormat::RGBA32F, 1280, 720, nullptr, props);
	}
	
	void SceneRenderer::createGridResources()
	{
		Ref<MaterialAsset> gridMaterialAsset = Renderer::GetDefaultResources().RendererAssets.at("GridMaterial").As<MaterialAsset>();

		m_GridMaterial = gridMaterialAsset->GetMaterial();
		m_GridMaterialInstance = gridMaterialAsset->GetMaterialInstance();


		m_GridMaterialInstance->Set("u_Settings.Scale", m_GridProps.Scale);
		m_GridMaterialInstance->Set("u_Settings.Size", m_GridProps.LineWidth);

		PipelineSpecification spec;
		spec.RenderPass = m_GeometryRenderPass;
		spec.Shader = m_GridMaterial->GetShader();
		spec.Topology = PrimitiveTopology::Triangles;
		spec.DepthTest = true;
		spec.DepthWrite = true;
		spec.BackfaceCulling = false;
		m_GridPipeline = Pipeline::Create(spec);
	}
	void SceneRenderer::updateViewportSize()
	{
		if (m_ViewportSizeChanged)
		{
			const uint32_t width = (uint32_t)m_ViewportSize.x;
			const uint32_t height = (uint32_t)m_ViewportSize.y;
			if (width != 0 && height != 0)
			{
				m_GeometryRenderPass->GetSpecification().TargetFramebuffer->Resize(width, height);
				m_LightRenderPass->GetSpecification().TargetFramebuffer->Resize(width, height);
				m_CompositeRenderPass->GetSpecification().TargetFramebuffer->Resize(width, height);
				m_DepthRenderPass->GetSpecification().TargetFramebuffer->Resize(width, height);

				TextureProperties props;
				props.Storage = true;
				props.SamplerWrap = TextureWrap::Clamp;
				// TODO: resizing
				m_BloomTexture[0] = Texture2D::Create(ImageFormat::RGBA32F, width, height, nullptr, props);
				m_BloomTexture[1] = Texture2D::Create(ImageFormat::RGBA32F, width, height, nullptr, props);
				m_BloomTexture[2] = Texture2D::Create(ImageFormat::RGBA32F, width, height, nullptr, props);
				m_BloomPass.SetBloomTextures(m_BloomTexture);

				m_LightCullingWorkGroups = {
					(m_ViewportSize.x + m_ViewportSize.x % 16) / 16,
					(m_ViewportSize.y + m_ViewportSize.y % 16) / 16,
					1
				};
				m_RendererDataUB.TilesCountX = m_LightCullingWorkGroups.x;


				const uint32_t lightCullingSize = 
						m_LightCullingWorkGroups.x
					*	m_LightCullingWorkGroups.y
					*	SSBOPointLights3D::MaxLights * sizeof(uint32_t);


				m_StorageBufferSet->Resize(lightCullingSize, SSBOLightCulling::Set, SSBOLightCulling::Binding);
			}
			m_ViewportSizeChanged = false;
		}
	}
	void SceneRenderer::preRender()
	{
		m_GeometryPassStatistics = m_GeometryPass.PreSubmit(m_Queue);
		DeferredLightPassStatistics lightPassStats = m_DeferredLightPass.PreSubmit(m_ActiveScene);
		
		m_RenderStatistics.MeshDrawCommandCount = static_cast<uint32_t>(m_Queue.MeshDrawCommands.size());
		m_RenderStatistics.MeshOverrideDrawCommandCount = m_GeometryPassStatistics.MeshOverrideCount;
		
		m_RenderStatistics.AnimatedMeshDrawCommandCount = static_cast<uint32_t>(m_Queue.AnimatedMeshDrawCommands.size());
		m_RenderStatistics.AnimatedMeshOverrideDrawCommandCount = m_GeometryPassStatistics.AnimatedMeshOverrideCount;

		m_RenderStatistics.InstanceMeshDrawCommandCount = static_cast<uint32_t>(m_Queue.InstanceMeshDrawCommands.size());
		
		m_RenderStatistics.TransformInstanceCount = m_GeometryPassStatistics.TransformInstanceCount;
		m_RenderStatistics.InstanceDataSize = m_GeometryPassStatistics.InstanceDataSize;
		m_RenderStatistics.SpriteDrawCommandCount = static_cast<uint32_t>(m_Queue.SpriteDrawCommands.size());	
	
		m_RenderStatistics.PointLight2DCount = lightPassStats.PointLightCount;
		m_RenderStatistics.SpotLight2DCount = lightPassStats.SpotLightCount;
	}

	void SceneRenderer::renderGrid()
	{
		const glm::mat4 transform = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(8.0f));

		Renderer::BindPipeline(m_CommandBuffer, m_GridPipeline, m_UniformBufferSet, nullptr, m_GridMaterial);
		Renderer::SubmitFullscreenQuad(m_CommandBuffer, m_GridPipeline, m_GridMaterialInstance, transform);
		
	}

	void SceneRenderer::updateBufferSets()
	{
		Ref<SceneRenderer> instance = this;

		Renderer::Submit([instance]() mutable {

			const uint32_t currentFrame = Renderer::GetCurrentFrame();
			instance->m_UniformBufferSet->Get(UBCameraData::Binding, UBCameraData::Set, currentFrame)->RT_Update(&instance->m_CameraDataUB, sizeof(UBCameraData), 0);
			instance->m_UniformBufferSet->Get(UBRendererData::Binding, UBRendererData::Set, currentFrame)->RT_Update(&instance->m_RendererDataUB, sizeof(UBRendererData), 0);
			instance->m_UniformBufferSet->Get(UBSceneData::Binding, UBSceneData::Set, currentFrame)->RT_Update(&instance->m_SceneDataUB, sizeof(UBSceneData), 0);
			instance->m_StorageBufferSet->Get(SSBOPointLights3D::Binding, SSBOPointLights3D::Set, currentFrame)->RT_Update(&instance->m_PointsLights3DSSBO, sizeof(SSBOPointLights3D), 0);
		});
	}
}