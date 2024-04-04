#pragma once
#include "Renderer2D.h"
#include "VoxelMesh.h"

#include "XYZ/Utils/Math/Ray.h"

namespace XYZ {

	namespace Utils {
		struct RaymarchResult
		{
			bool Hit = false;
			glm::vec4 Color;
			float Distance;
		};

		struct RaymarchState
		{
			glm::vec4  Color;
			glm::vec3  Max;
			glm::ivec3 CurrentVoxel;
			glm::ivec3 MaxSteps;
			bool  Hit;
			float Distance;
			glm::ivec3 DecompressedVoxelOffset;
		};
	}
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

		void RaymarchSubmesh(
			uint32_t submeshIndex,
			const AABB& boundingBox,
			const glm::mat4& transform,
			const Ref<VoxelMesh>& mesh,
			const glm::vec2& coords
		);

		void SetViewportSize(uint32_t width, uint32_t height);

		Ref<Image2D> GetFinalImage() const { return m_ColorRenderPass->GetSpecification().TargetFramebuffer->GetImage(); }

		bool UpdateCamera = true;
	private:
		void render();
		void renderColor();
		void renderLines();

		void createRenderPass();
		void createPipeline();
		void updateViewportsize();

		void submitAABB(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color);
		void submitRay(const Ray& ray, float size, const glm::vec4& color);
		void submitCompressedVoxels(const VoxelSubmesh& submesh, const std::array<VoxelColor, 256>& collorPallete);
		void submitVoxelCell(const VoxelSubmesh& submesh, const std::array<VoxelColor, 256>& collorPallete, const glm::ivec3& cellCoord);

		void rayMarch(const Ray& ray, Utils::RaymarchState& state, const glm::vec3& delta, const glm::ivec3& step, uint32_t width, uint32_t height, uint32_t depth, uint32_t voxelOffset, const VoxelSubmesh& model, float currentDistance, const std::array<VoxelColor, 256>& colorPallete);
		Utils::RaymarchResult rayMarchSteps(const Ray& ray, const glm::vec4& startColor, float tMin, uint32_t width, uint32_t height, uint32_t depth, uint32_t voxelOffset, const VoxelSubmesh& model, float currentDistance, const glm::ivec3& maxSteps, const glm::ivec3& decompressedVoxelOffset, const std::array<VoxelColor, 256>& colorPallete);
		float raymarchCompressed(const Ray& ray, const glm::vec4& startColor, float tMin, const VoxelSubmesh& model, float currentDistance, const std::array<VoxelColor, 256>& colorPallete);
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
		glm::mat4 m_Transform;

		struct Line
		{
			glm::vec3 P0, P1;
			glm::vec4 Color;
		};
		std::vector<Line> m_DebugLines;

		const glm::vec4 c_BoundingBoxColor;
	};

}
