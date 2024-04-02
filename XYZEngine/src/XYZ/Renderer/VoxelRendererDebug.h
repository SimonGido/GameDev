#pragma once
#include "Renderer2D.h"


namespace XYZ {
	
	class VoxelRendererDebug
	{
	public:
		VoxelRendererDebug(
			Ref<PrimaryRenderCommandBuffer> commandBuffer,
			Ref<UniformBufferSet> uniformBufferSet
		);

		void BeginScene(
			const glm::mat4& inverseViewMatrix,
			const glm::mat4& inverseProjection,
			const glm::vec3& cameraPosition
		);
		void EndScene(Ref<Image2D> image);

		void SetViewportSize(uint32_t width, uint32_t height);

		Ref<Image2D> GetFinalImage() const { return m_ColorRenderPass->GetSpecification().TargetFramebuffer->GetImage(); }

	private:
		void render();
		void renderColor();
		void renderLines();

		void createRenderPass();
		void createPipeline();
		void updateViewportsize();

	private:
		glm::ivec2 m_ViewportSize;
		bool	   m_ViewportSizeChanged = false;

		Ref<PrimaryRenderCommandBuffer> m_CommandBuffer;
		Ref<UniformBufferSet>			m_UniformBufferSet;

		Ref<Renderer2D> m_Renderer;
		Ref<RenderPass> m_ColorRenderPass;


		Ref<Pipeline>		  m_LinePipeline;
		Ref<Material>		  m_LineMaterial;
		Ref<MaterialInstance> m_LineMaterialInstance;

		Ref<Pipeline>		  m_ColorPipeline;
		Ref<Material>		  m_ColorMaterial;
		Ref<MaterialInstance> m_ColorMaterialInstance;


		glm::mat4 m_InverseViewMatrix;
		glm::mat4 m_InverseProjection;
		glm::vec3 m_CameraPosition;
	};

}
