#include "stdafx.h"
#include "SceneRenderer.h"

#include "Renderer2D.h"
#include "Renderer.h"

#include "XYZ/Core/Input.h"
#include "XYZ/Debug/Profiler.h"
#include "XYZ/ImGui/ImGui.h"
#include "XYZ/API/Vulkan/VulkanRendererAPI.h"
#include "XYZ/API/Vulkan/VulkanPipelineCompute.h"

#include <glm/gtx/transform.hpp>

#include <imgui/imgui.h>

namespace XYZ {

	static ThreadPool s_ThreadPool;

	SceneRenderer::SceneRenderer(Ref<Scene> scene, SceneRendererSpecification specification)
		:
		m_Specification(specification),
		m_ActiveScene(scene)
	{
		m_ViewportSize = { 1280, 720 };
		Init();
	}

	SceneRenderer::~SceneRenderer()
	{
		s_ThreadPool.EraseThread(m_ThreadIndex);
	}


	void SceneRenderer::Init()
	{
		XYZ_PROFILE_FUNC("SceneRenderer::Init");
		m_ThreadIndex = s_ThreadPool.PushThread();
		if (m_Specification.SwapChainTarget)
			m_CommandBuffer = Renderer::GetAPIContext()->GetRenderCommandBuffer();
		else
			m_CommandBuffer = RenderCommandBuffer::Create(0, "SceneRenderer");

		m_CommandBuffer->CreateTimestampQueries(GPUTimeQueries::Count());

		m_Renderer2D = Ref<Renderer2D>::Create(m_CommandBuffer);
	

		createCompositePass();
		createLightPass();
		auto shaderLibrary = Renderer::GetShaderLibrary();
		const BufferLayout layout = { 
			{0, ShaderDataType::Float3, "a_Position" },
			{1, ShaderDataType::Float2, "a_TexCoord" } 
		};
		m_CompositeRenderPipeline.Init(m_CompositePass, shaderLibrary->Get("CompositeShader"), layout);
		m_LightRenderPipeline.Init(m_LightPass, shaderLibrary->Get("LightShader"), layout);

		
		// Geometry pass
		{
			FramebufferSpecification specs;
			specs.ClearColor = { 0.0f,0.0f,0.0f,0.0f };
			specs.Attachments = {
				FramebufferTextureSpecification(ImageFormat::RGBA32F),
				FramebufferTextureSpecification(ImageFormat::RGBA32F),
				FramebufferTextureSpecification(ImageFormat::DEPTH24STENCIL8)
			};
			Ref<Framebuffer> fbo = Framebuffer::Create(specs);
			m_GeometryPass = RenderPass::Create({ fbo });
		}
		// Bloom pass
		{
			FramebufferSpecification specs;
			specs.ClearColor = { 0.0f,0.0f,0.0f, 0.0f };
			specs.Attachments = {
				FramebufferTextureSpecification(ImageFormat::RGBA16F),
				FramebufferTextureSpecification(ImageFormat::DEPTH24STENCIL8)
			};
			Ref<Framebuffer> fbo = Framebuffer::Create(specs);
			m_BloomPass = RenderPass::Create({ fbo });
		}

		auto bloomShader = shaderLibrary->Get("Bloom");
		m_BloomComputePipeline = PipelineCompute::Create(bloomShader);
		m_BloomComputeMaterial = Material::Create(bloomShader);

		TextureProperties props;
		props.Storage = true;
		props.SamplerWrap = TextureWrap::Clamp;
		m_BloomTexture[0] = Texture2D::Create(ImageFormat::RGBA32F, 1280, 720, nullptr, props);
		m_BloomTexture[1] = Texture2D::Create(ImageFormat::RGBA32F, 1280, 720, nullptr, props);
		m_BloomTexture[2] = Texture2D::Create(ImageFormat::RGBA32F, 1280, 720, nullptr, props);
		
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

		// Viewport size is changed at the beginning of the frame, so we do not delete texture that is currently use for rendering
		updateViewportSize();
		m_CameraBuffer.ViewProjectionMatrix = m_SceneCamera.Camera.GetProjectionMatrix() * m_SceneCamera.ViewMatrix;
		m_CameraBuffer.ViewMatrix = m_SceneCamera.ViewMatrix;
		m_CameraBuffer.ViewPosition = glm::vec4(camera.ViewPosition, 0.0f);
		
		//m_CameraUniformBuffer->Update(&m_CameraBuffer, sizeof(CameraData), 0);
	}
	void SceneRenderer::BeginScene(const glm::mat4& viewProjectionMatrix, const glm::mat4& viewMatrix, const glm::vec3& viewPosition)
	{
		// Viewport size is changed at the beginning of the frame, so we do not delete texture that is currently use for rendering
		updateViewportSize();

		m_CameraBuffer.ViewProjectionMatrix = viewProjectionMatrix;
		m_CameraBuffer.ViewMatrix = viewMatrix;
		m_CameraBuffer.ViewPosition = glm::vec4(viewPosition, 0.0f);

		//m_CameraUniformBuffer->Update(&m_CameraBuffer, sizeof(CameraData), 0);		
	}
	void SceneRenderer::EndScene()
	{
		m_CommandBuffer->Begin();
		m_GPUTimeQueries.GPUTime = m_CommandBuffer->BeginTimestampQuery();
		
		flushDefaultQueue();
		flushLightQueue();

		// Composite pass
		Ref<Image2D> lightPassImage = m_LightPass->GetSpecification().TargetFramebuffer->GetImage();
		Renderer::BeginRenderPass(m_CommandBuffer, m_CompositePass, true);
		m_CompositeRenderPipeline.Material->SetImage("u_GeometryTexture", lightPassImage);
		m_CompositeRenderPipeline.Material->SetImage("u_BloomTexture", m_BloomTexture[2]->GetImage());
		Renderer::BindPipeline(
			m_CommandBuffer,
			m_CompositeRenderPipeline.Pipeline,
			nullptr,
			nullptr,
			m_CompositeRenderPipeline.Material
		);

		Renderer::SubmitFullscreenQuad(m_CommandBuffer, m_CompositeRenderPipeline.Pipeline, m_CompositeRenderPipeline.Material);
		Renderer::EndRenderPass(m_CommandBuffer);
		

		m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.GPUTime);

		m_CommandBuffer->End();
		m_CommandBuffer->Submit();
	}


	void SceneRenderer::SubmitBillboard(const Ref<Material>& material, const Ref<SubTexture>& subTexture, uint32_t sortLayer, const glm::vec4& color, const glm::vec3& position, const glm::vec2& size)
	{
		m_Queue.m_BillboardDrawList.push_back({
			   material,subTexture, sortLayer, color, position, size
			});
	}

	void SceneRenderer::SubmitSprite(const Ref<Material>& material, const Ref<SubTexture>& subTexture, const glm::vec4& color, const glm::mat4& transform)
	{
		m_Queue.m_SpriteDrawList.push_back({ material, subTexture, color, transform });
	}
	void SceneRenderer::SubmitMesh(const Ref<Mesh>& mesh, const glm::mat4& transform)
	{
		//m_Queues[mesh->GetMaterial()->GetRenderQueueID()].m_MeshCommandList.push_back({ mesh, transform });
	}
	void SceneRenderer::SubmitMeshInstanced(const Ref<Mesh>& mesh, const glm::mat4& transform, uint32_t count)
	{
		//m_Queues[mesh->GetMaterial()->GetRenderQueueID()].m_InstancedMeshCommandList.push_back({ mesh, transform, count });
	}
	void SceneRenderer::SubmitMeshInstanced(const Ref<Mesh>& mesh, const std::vector<glm::mat4>& transforms, uint32_t count)
	{
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
			uint32_t frameIndex = Renderer::GetCurrentFrame();
			if (UI::BeginTreeNode("GPU measurements"))
			{
				if (ImGui::BeginTable("##GPUTime", 2, ImGuiTableFlags_SizingFixedFit))
				{
					UI::TextTableRow("%s", "GPU time:", "%.3fms", m_CommandBuffer->GetExecutionGPUTime(frameIndex, m_GPUTimeQueries.GPUTime));
					UI::TextTableRow("%s", "Renderer2D Pass:", "%.3fms", m_CommandBuffer->GetExecutionGPUTime(frameIndex, m_GPUTimeQueries.Renderer2DPassQuery));
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
		}
		ImGui::End();
	}

	Ref<RenderPass> SceneRenderer::GetFinalRenderPass() const
	{	
		// TODO: temporary;
		return m_Renderer2D->GetTargetRenderPass();
		//return m_CompositePipeline->GetSpecification().RenderPass;
	}

	Ref<Image2D> SceneRenderer::GetFinalPassImage() const
	{
		//return m_BloomTexture[2]->GetImage();
		return m_LightPass->GetSpecification().TargetFramebuffer->GetImage();
		//
		//return m_Renderer2D->GetTargetRenderPass()->GetSpecification().TargetFramebuffer->GetImage();
		return m_CompositeRenderPipeline.Pipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage();
	}
	SceneRendererOptions& SceneRenderer::GetOptions()
	{
		return m_Options;
	}
	void SceneRenderer::flush()
	{
		XYZ_PROFILE_FUNC("SceneRenderer::flush");
		//flushLightQueue();
		
		
		//Renderer::BeginRenderPass(m_CommandBuffer, m_CompositePipeline->GetSpecification().RenderPass, true);
		//
		////m_LightPass->GetSpecification().TargetFramebuffer->BindTexture(0, 0);
		////m_BloomTexture[2]->Bind(1);
		//
		//auto geometryImage = m_Renderer2D->GetTargetRenderPass()->GetSpecification().TargetFramebuffer->GetImage();
		//
		//m_CompositeMaterial->Set("u_GeometryTexture", geometryImage);
		//m_CompositeMaterial->Set("u_BloomTexture", geometryImage);
		//Renderer::BindPipeline(m_CommandBuffer, m_CompositePipeline, nullptr, m_CompositeMaterial);
		//Renderer::SubmitFullscreenQuad(m_CommandBuffer, m_CompositePipeline, m_CompositeMaterial);
		//
		//Renderer::EndRenderPass(m_CommandBuffer);

		//auto [width, height] = Input::GetWindowSize();
		//Renderer::SetViewPort(0, 0, (uint32_t)width, (uint32_t)height);
	}
	void SceneRenderer::flushLightQueue()
	{
		XYZ_PROFILE_FUNC("SceneRenderer::flushLightQueue");

		auto& ecs = m_ActiveScene->GetECS();
		m_NumSpotLights = ecs.GetStorage<SpotLight2D>().Size();
		m_NumPointLights = ecs.GetStorage<PointLight2D>().Size();

		std::vector<SpotLight> spotLights;
		spotLights.reserve(m_NumSpotLights);
		auto spotLight2DView = ecs.CreateView<TransformComponent, SpotLight2D>();
		for (auto entity : spotLight2DView)
		{
			// Render previous frame data
			auto &[transform, light] = spotLight2DView.Get<TransformComponent, SpotLight2D>(entity);
			auto [trans, rot, scale] = transform.GetWorldComponents();

			SpotLight lightData;
			lightData.Color		 = glm::vec4(light.Color, 1.0f);
			lightData.Position   = trans;
			lightData.Radius	 = light.Radius;
			lightData.Intensity  = light.Intensity;
			lightData.InnerAngle = light.InnerAngle;
			lightData.OuterAngle = light.OuterAngle;

			spotLights.push_back(lightData);
		}

		std::vector<PointLight> pointLights;
		pointLights.reserve(m_NumPointLights);
		auto pointLight2DView = ecs.CreateView<TransformComponent, PointLight2D>();
		for (auto entity : pointLight2DView)
		{
			auto &[transform, light] = pointLight2DView.Get<TransformComponent, PointLight2D>(entity);
			auto [trans, rot, scale] = transform.GetWorldComponents();
			PointLight lightData;
			lightData.Color = glm::vec4(light.Color, 1.0f);
			lightData.Position = trans;
			lightData.Radius = light.Radius;
			lightData.Intensity = light.Intensity;
			pointLights.push_back(lightData);
		}

		Ref<StorageBufferSet> instance = m_LightStorageBufferSet;
		Renderer::Submit([instance, pLights = std::move(pointLights), sLights = std::move(spotLights)]() mutable {
			const uint32_t frame = Renderer::GetCurrentFrame();
			instance->Get(1, 0, frame)->RT_Update(pLights.data(), pLights.size() * sizeof(PointLight));
			instance->Get(2, 0, frame)->RT_Update(sLights.data(), sLights.size() * sizeof(SpotLight));
		});

		
		lightPass();
		bloomPass();

		//queue.m_SpriteDrawList.clear();
		//queue.m_MeshCommandList.clear();
		//queue.m_InstancedMeshCommandList.clear();
	}
	void SceneRenderer::flushDefaultQueue()
	{
		XYZ_PROFILE_FUNC("SceneRenderer::flushDefaultQueue");
		geometryPass2D(m_Queue, true);
	
		m_Queue.m_SpriteDrawList.clear();
		m_Queue.m_BillboardDrawList.clear();
		m_Queue.m_MeshCommandList.clear();
		m_Queue.m_InstancedMeshCommandList.clear();
	}
	
	void SceneRenderer::geometryPass2D(RenderQueue& queue, bool clear)
	{
		m_GPUTimeQueries.Renderer2DPassQuery = m_CommandBuffer->BeginTimestampQuery();
		m_Renderer2D->BeginScene(m_CameraBuffer.ViewProjectionMatrix, m_CameraBuffer.ViewMatrix);
		
		for (auto& data : queue.m_SpriteDrawList)
		{
			m_Renderer2D->SetQuadMaterial(data.Material);
			m_Renderer2D->SubmitQuad(data.Transform, data.SubTexture, data.Color);
		}
		for (auto& dc : queue.m_BillboardDrawList)
		{
			m_Renderer2D->SetQuadMaterial(dc.Material);
			m_Renderer2D->SubmitQuadBillboard(dc.Position, dc.Size, dc.SubTexture, dc.Color);
		}

		m_Renderer2D->EndScene();
		m_CommandBuffer->EndTimestampQuery(m_GPUTimeQueries.Renderer2DPassQuery);
	}
	void SceneRenderer::lightPass()
	{
		Renderer::BeginRenderPass(m_CommandBuffer, m_LightPass, true);

		Ref<Framebuffer>& renderer2DFramebuffer = m_Renderer2D->GetTargetRenderPass()->GetSpecification().TargetFramebuffer;
		Ref<Image2D> geometryColorImage = renderer2DFramebuffer->GetImage(0);
		Ref<Image2D> geometryPositionImage = renderer2DFramebuffer->GetImage(1);

		m_LightRenderPipeline.Material->SetImageArray("u_Texture", geometryColorImage, 0);
		m_LightRenderPipeline.Material->SetImageArray("u_Texture", geometryPositionImage, 1);

		m_LightRenderPipeline.Material->Set("u_Uniforms.NumberPointLights", (int)m_NumPointLights);
		m_LightRenderPipeline.Material->Set("u_Uniforms.NumberSpotLights", (int)m_NumSpotLights);

		

		Renderer::BindPipeline(
			m_CommandBuffer, 
			m_LightRenderPipeline.Pipeline, 
			m_CameraUniformBuffer, 
			m_LightStorageBufferSet, 
			m_LightRenderPipeline.Material
		);
		
		Renderer::SubmitFullscreenQuad(m_CommandBuffer, m_LightRenderPipeline.Pipeline, m_LightRenderPipeline.Material);
		Renderer::EndRenderPass(m_CommandBuffer);
	}

	void SceneRenderer::bloomPass()
	{
		constexpr int prefilter = 0;
		constexpr int downsample = 1;
		constexpr int upsamplefirst = 2;
		constexpr int upsample = 3;

		auto vulkanPipeline = m_BloomComputePipeline;

		auto imageBarrier = [](Ref<VulkanPipelineCompute> pipeline, Ref<VulkanImage2D> image) {

			Renderer::Submit([pipeline, image]() {
				VkImageMemoryBarrier imageMemoryBarrier = {};
				imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
				imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
				imageMemoryBarrier.image = image->GetImageInfo().Image;
				imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, image->GetSpecification().Mips, 0, 1 };
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				vkCmdPipelineBarrier(
					pipeline->GetActiveCommandBuffer(),
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					0,
					0, nullptr,
					0, nullptr,
					1, &imageMemoryBarrier);
			});
		};

		const uint32_t workGroupSize = 4;
		uint32_t workGroupsX = (uint32_t)glm::ceil(m_ViewportSize.x / workGroupSize);
		uint32_t workGroupsY = (uint32_t)glm::ceil(m_ViewportSize.y / workGroupSize);
		Ref<Image2D> lightPassImage = m_LightPass->GetSpecification().TargetFramebuffer->GetImage();
		// Renderer::ClearImage(m_CommandBuffer, m_BloomTexture[2]->GetImage());

		Ref<Material> computeMaterial = m_BloomComputeMaterial;
		Renderer::Submit([computeMaterial, bloomSettings = m_BloomSettings, prefilter]() mutable {
			computeMaterial->Set("u_Uniforms.FilterTreshold", bloomSettings.FilterTreshold);
			computeMaterial->Set("u_Uniforms.FilterKnee", bloomSettings.FilterKnee);
			computeMaterial->Set("u_Uniforms.Mode", prefilter);
			//computeMaterial->Set("u_Uniforms.LOD", 0.0f);			
		});
		computeMaterial->SetImage("o_Image", m_BloomTexture[0]->GetImage(), 0);
		computeMaterial->SetImage("u_Texture", lightPassImage);
		computeMaterial->SetImage("u_BloomTexture", lightPassImage);

		Renderer::BeginPipelineCompute(m_CommandBuffer, m_BloomComputePipeline, nullptr, nullptr, m_BloomComputeMaterial);		
		Renderer::DispatchCompute(m_BloomComputePipeline, m_BloomComputeMaterial, workGroupsX, workGroupsY, 1);
		imageBarrier(vulkanPipeline, m_BloomTexture[0]->GetImage());

		Renderer::Submit([computeMaterial, downsample]() mutable {
			computeMaterial->Set("u_Uniforms.Mode", downsample);
		});

		const uint32_t mips = m_BloomTexture[0]->GetMipLevelCount() - 2;
		for (uint32_t mip = 1; mip < mips; ++mip)
		{
			auto [mipWidth, mipHeight] = m_BloomTexture[0]->GetMipSize(mip);

			computeMaterial->SetImage("o_Image", m_BloomTexture[1]->GetImage(), mip);
			computeMaterial->SetImage("u_Texture", m_BloomTexture[0]->GetImage());
			Renderer::Submit([computeMaterial, mip]() mutable {

				computeMaterial->Set("u_Uniforms.LOD", mip - 1.0f);
			});

			workGroupsX = (uint32_t)glm::ceil((float)mipWidth / (float)workGroupSize);
			workGroupsY = (uint32_t)glm::ceil((float)mipHeight / (float)workGroupSize);
			
			Renderer::UpdateDescriptors(m_BloomComputePipeline, m_BloomComputeMaterial, nullptr, nullptr);
			Renderer::DispatchCompute(m_BloomComputePipeline, m_BloomComputeMaterial, workGroupsX, workGroupsY, 1);
			imageBarrier(vulkanPipeline, m_BloomTexture[1]->GetImage());
		}
		Renderer::Submit([computeMaterial, mips, upsamplefirst]() mutable {
			computeMaterial->Set("u_Uniforms.Mode", upsamplefirst);
			computeMaterial->Set("u_Uniforms.LOD", mips - 2.0f);
		});
		
		m_BloomComputeMaterial->SetImage("o_Image", m_BloomTexture[2]->GetImage(), mips - 2);
		m_BloomComputeMaterial->SetImage("u_Texture", m_BloomTexture[0]->GetImage());
		
		auto [mipWidth, mipHeight] = m_BloomTexture[2]->GetMipSize(mips - 2);
		workGroupsX = (uint32_t)glm::ceil((float)mipWidth / (float)workGroupSize);
		workGroupsY = (uint32_t)glm::ceil((float)mipHeight / (float)workGroupSize);
		
		Renderer::UpdateDescriptors(m_BloomComputePipeline, m_BloomComputeMaterial, nullptr, nullptr);
		Renderer::DispatchCompute(m_BloomComputePipeline, m_BloomComputeMaterial, workGroupsX, workGroupsY, 1);	
		imageBarrier(vulkanPipeline, m_BloomTexture[2]->GetImage());
		
		// Upsample stage
		Renderer::Submit([computeMaterial, upsample]() mutable {
			computeMaterial->Set("u_Uniforms.Mode", upsample);
		});

		for (int32_t mip = mips - 3; mip >= 0; mip--)
		{
			auto [mipWidth, mipHeight] = m_BloomTexture[2]->GetMipSize(mip);
			workGroupsX = (uint32_t)glm::ceil((float)mipWidth / (float)workGroupSize);
			workGroupsY = (uint32_t)glm::ceil((float)mipHeight / (float)workGroupSize);
		
			m_BloomComputeMaterial->SetImage("o_Image", m_BloomTexture[2]->GetImage(), mip);
			m_BloomComputeMaterial->SetImage("u_Texture", m_BloomTexture[0]->GetImage());
			m_BloomComputeMaterial->SetImage("u_BloomTexture", m_BloomTexture[2]->GetImage());
			Renderer::Submit([computeMaterial, mip]() mutable {

				computeMaterial->Set("u_Uniforms.LOD", mip);
			});
		
			Renderer::UpdateDescriptors(m_BloomComputePipeline, m_BloomComputeMaterial, nullptr, nullptr);
			Renderer::DispatchCompute(m_BloomComputePipeline, m_BloomComputeMaterial, workGroupsX, workGroupsY, 1);
			imageBarrier(vulkanPipeline, m_BloomTexture[2]->GetImage());
		}
		
		Renderer::EndPipelineCompute(m_BloomComputePipeline);
	}
	void SceneRenderer::createCompositePass()
	{
		FramebufferSpecification compFramebufferSpec;
		compFramebufferSpec.ClearColor = { 0.1f, 0.1f, 0.1f, 1.0f };
		compFramebufferSpec.SwapChainTarget = m_Specification.SwapChainTarget;

		// No depth for swapchain
		if (m_Specification.SwapChainTarget)
			compFramebufferSpec.Attachments = { ImageFormat::RGBA };
		else
			compFramebufferSpec.Attachments = { ImageFormat::RGBA, ImageFormat::Depth };

		Ref<Framebuffer> framebuffer = Framebuffer::Create(compFramebufferSpec);

		RenderPassSpecification renderPassSpec;
		renderPassSpec.TargetFramebuffer = framebuffer;
		m_CompositePass = RenderPass::Create(renderPassSpec);;
	}
	void SceneRenderer::createLightPass()
	{
		auto shaderLibrary = Renderer::GetShaderLibrary();
		auto shader = shaderLibrary->Get("LightShader");
		FramebufferSpecification specs;
		specs.ClearColor = { 0.0f,0.0f,0.0f,0.0f };
		specs.Attachments = {
			FramebufferTextureSpecification(ImageFormat::RGBA32F, true)
		};
		Ref<Framebuffer> fbo = Framebuffer::Create(specs);
		m_LightPass = RenderPass::Create({ fbo });

		m_LightStorageBufferSet = StorageBufferSet::Create(Renderer::GetConfiguration().FramesInFlight);
		m_LightStorageBufferSet->Create(sc_MaxNumberOfLights * sizeof(PointLight), 0, 1);
		m_LightStorageBufferSet->Create(sc_MaxNumberOfLights * sizeof(SpotLight), 0, 2);
		m_LightStorageBufferSet->CreateDescriptors(shader);
	}
	

	void SceneRenderer::updateViewportSize()
	{
		if (m_ViewportSizeChanged)
		{
			const uint32_t width = (uint32_t)m_ViewportSize.x;
			const uint32_t height = (uint32_t)m_ViewportSize.y;
			m_GeometryPass->GetSpecification().TargetFramebuffer->Resize(width, height);
			m_LightPass->GetSpecification().TargetFramebuffer->Resize(width, height);
			m_BloomPass->GetSpecification().TargetFramebuffer->Resize(width, height);

			TextureProperties props;
			props.Storage = true;
			props.SamplerWrap = TextureWrap::Clamp;
			// TODO: resizing
			m_BloomTexture[0] = Texture2D::Create(ImageFormat::RGBA32F, width, height, nullptr, props);
			m_BloomTexture[1] = Texture2D::Create(ImageFormat::RGBA32F, width, height, nullptr, props);
			m_BloomTexture[2] = Texture2D::Create(ImageFormat::RGBA32F, width, height, nullptr, props);
			m_ViewportSizeChanged = false;
		}
	}
	void SceneRenderer::SceneRenderPipeline::Init(const Ref<RenderPass>& renderPass, const Ref<Shader>& shader, const BufferLayout& layout, PrimitiveTopology topology)
	{
		PipelineSpecification specification;
		specification.Shader = shader;
		specification.Layout = layout;
		specification.RenderPass = renderPass;
		specification.Topology = topology;
		this->Pipeline = Pipeline::Create(specification);
		this->Material = Material::Create(shader);
	}
}