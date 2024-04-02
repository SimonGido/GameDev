#include "stdafx.h"
#include "VoxelRendererDebug.h"

#include "Renderer.h"

namespace XYZ {
	VoxelRendererDebug::VoxelRendererDebug(
		Ref<PrimaryRenderCommandBuffer> commandBuffer,
		Ref<UniformBufferSet> uniformBufferSet
	)
		:
		m_CommandBuffer(commandBuffer),
		m_UniformBufferSet(uniformBufferSet)
	{
		m_ViewportSize = { 1280, 720 };
		m_Renderer = Ref<Renderer2D>::Create(Renderer2DConfiguration{ commandBuffer, uniformBufferSet });
		createRenderPass();
		createPipeline();
	}
	
	void VoxelRendererDebug::BeginScene(const glm::mat4& inverseViewMatrix, const glm::mat4& inverseProjection, const glm::vec3& cameraPosition)
	{
		m_InverseViewMatrix = inverseViewMatrix;
		m_InverseProjection = inverseProjection;
		m_CameraPosition = cameraPosition;
		updateViewportsize();
	}
	void VoxelRendererDebug::EndScene(Ref<Image2D> image)
	{
		m_ColorMaterial->SetImage("u_ColorTexture", image);
		render();
	}
	void VoxelRendererDebug::SetViewportSize(uint32_t width, uint32_t height)
	{
		if (m_ViewportSize.x != width || m_ViewportSize.y != height)
		{
			m_ViewportSize = glm::ivec2(width, height);
			m_ViewportSizeChanged = true;
		}
	}
	void VoxelRendererDebug::render()
	{
		Renderer::BeginRenderPass(
			m_CommandBuffer,
			m_ColorRenderPass,
			false,
			true
		);

		renderColor();
		renderLines();
		
		m_Renderer->EndScene();

		Renderer::EndRenderPass(m_CommandBuffer);
	}
	void VoxelRendererDebug::renderColor()
	{
		Renderer::BindPipeline(
			m_CommandBuffer,
			m_ColorPipeline,
			m_UniformBufferSet,
			nullptr,
			m_ColorMaterial
		);

		Renderer::SubmitFullscreenQuad(m_CommandBuffer, m_ColorPipeline, m_ColorMaterialInstance);
	}
	void VoxelRendererDebug::renderLines()
	{
		Renderer::BindPipeline(
			m_CommandBuffer,
			m_LinePipeline,
			m_UniformBufferSet,
			nullptr,
			m_LineMaterial
		);

		m_Renderer->BeginScene(glm::inverse(m_InverseViewMatrix));

		m_Renderer->SubmitLine(glm::vec3(0, 0, 0), glm::vec3(10, 10, 10));
		m_Renderer->SubmitCircle(glm::vec3(0, 0, 0), 10, 10);

		m_Renderer->FlushLines(m_LinePipeline, m_LineMaterialInstance, true);
	}
	void VoxelRendererDebug::createRenderPass()
	{
		FramebufferSpecification framebufferSpec;
		framebufferSpec.Attachments = {
				FramebufferTextureSpecification(ImageFormat::RGBA32F)
		};
		framebufferSpec.Samples = 1;
		framebufferSpec.ClearOnLoad = false;
		framebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 0.0f };

		Ref<Framebuffer> framebuffer = Framebuffer::Create(framebufferSpec);

		RenderPassSpecification renderPassSpec;
		renderPassSpec.TargetFramebuffer = framebuffer;
		m_ColorRenderPass = RenderPass::Create(renderPassSpec);
	}
	void VoxelRendererDebug::createPipeline()
	{
		{ // Line pipeline
			Ref<Shader> shader = Shader::Create("Resources/Shaders/Voxel/VoxelDebugLineShader.glsl");
			m_LineMaterial = Material::Create(shader);
			m_LineMaterialInstance = m_LineMaterial->CreateMaterialInstance();

			PipelineSpecification spec;
			spec.RenderPass = m_ColorRenderPass;
			spec.Shader = m_LineMaterial->GetShader();
			spec.Topology = PrimitiveTopology::Lines;
			spec.DepthTest = true;
			spec.DepthWrite = true;
			m_LinePipeline = Pipeline::Create(spec);
		}
		{
			// Color pipeline
			Ref<Shader> shader = Shader::Create("Resources/Shaders/Voxel/VoxelDebugColorShader.glsl");
			m_ColorMaterial = Material::Create(shader);
			m_ColorMaterialInstance = m_ColorMaterial->CreateMaterialInstance();

			PipelineSpecification spec;
			spec.RenderPass = m_ColorRenderPass;
			spec.Shader = m_ColorMaterial->GetShader();
			spec.Topology = PrimitiveTopology::Triangles;
			spec.DepthTest = true;
			spec.DepthWrite = true;
			m_ColorPipeline = Pipeline::Create(spec);
		}
	}
	void VoxelRendererDebug::updateViewportsize()
	{
		if (m_ViewportSizeChanged)
		{
			const uint32_t width = (uint32_t)m_ViewportSize.x;
			const uint32_t height = (uint32_t)m_ViewportSize.y;
			
			m_ColorRenderPass->GetSpecification().TargetFramebuffer->Resize(width, height);
			m_ViewportSizeChanged = false;
		}
	}
}
