#pragma once

#include "Camera.h"
#include "Material.h"
#include "Mesh.h"
#include "SkeletalMesh.h"
#include "Font.h"
#include "Pipeline.h"
#include "RenderCommandBuffer.h"
#include "UniformBufferSet.h"
#include "SubTexture.h"

#include <glm/glm.hpp>

namespace XYZ {
	struct Renderer2DStats
	{
		uint32_t DrawCalls = 0;
		uint32_t LineDrawCalls = 0;
		uint32_t CollisionDrawCalls = 0;
		uint32_t FilledCircleDrawCalls = 0;
	};


	template <typename VertexType>
	struct Renderer2DBuffer
	{
		Renderer2DBuffer() = default;
		~Renderer2DBuffer();

		void Init(const Ref<RenderPass>& renderPass, const Ref<Shader>& shader, uint32_t maxVertices, uint32_t* indices, uint32_t indexCount, const BufferLayout& layout);
		void Reset();

		Ref<Pipeline>	  Pipeline;
		Ref<IndexBuffer>  IndexBuffer;
		Ref<VertexBuffer> VertexBuffer;
		VertexType*		  BufferBase = nullptr;
		VertexType*		  BufferPtr  = nullptr;
		uint32_t		  IndexCount = 0;
	};

	class Renderer2D : public RefCount
	{
	public:
		Renderer2D(const Ref<RenderCommandBuffer>& commandBuffer);
		~Renderer2D();

		void BeginScene(const glm::mat4& viewProjectionMatrix, const glm::mat4& viewMatrix);

		void SetQuadMaterial(const Ref<Material>& material);
		void SetLineMaterial(const Ref<Material>& material);
		void SetCircleMaterial(const Ref<Material>& material);

		void SetTargetRenderPass(const Ref<RenderPass>& renderPass);
		Ref<RenderPass> GetTargetRenderPass() const;


		void SubmitCircle(const glm::vec3& pos, float radius, uint32_t sides, const glm::vec4& color = glm::vec4(1.0f));
		void SubmitFilledCircle(const glm::vec3& pos, float radius, float thickness, const glm::vec4& color = glm::vec4(1.0f));
		
		void SubmitQuad(const glm::mat4& transform, const glm::vec4& color, float tilingFactor = 1.0f);
		void SubmitQuad(const glm::mat4& transform, const glm::vec4& texCoord, const Ref<Texture2D>& texture, const glm::vec4& color = glm::vec4(1), float tilingFactor = 1.0f);
		void SubmitQuad(const glm::vec3& position,	const glm::vec2& size, const glm::vec4& texCoord, const Ref<Texture2D>& texture, const glm::vec4& color, float tilingFactor = 1.0f);
		
		void SubmitQuad(const glm::mat4& transform, const Ref<SubTexture>& subTexture, const glm::vec4& color = glm::vec4(1), float tilingFactor = 1.0f);
		void SubmitQuad(const glm::vec3& position, const glm::vec2& size, const Ref<SubTexture>& subTexture, const glm::vec4& color, float tilingFactor = 1.0f);

		void SubmitQuadBillboard(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);
		void SubmitQuadBillboard(const glm::vec3& position, const glm::vec2& size, const glm::vec4& texCoord, const Ref<Texture2D>& texture, const glm::vec4& color = glm::vec4(1.0f), float tilingFactor = 1.0f);
		void SubmitQuadBillboard(const glm::vec3& position, const glm::vec2& size, const Ref<SubTexture>& subTexture, const glm::vec4& color = glm::vec4(1.0f), float tilingFactor = 1.0f);


		void SubmitQuad(const glm::vec3& position,	const glm::vec2& size, const glm::vec4& color, float tilingFactor);
		void SubmitQuadNotCentered(const glm::vec3& position, const glm::vec2& size, const glm::vec4& texCoord, const Ref<Texture2D>& texture, const glm::vec4& color = glm::vec4(1), float tilingFactor = 1.0f);

		void SubmitLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color = glm::vec4(1.0f));
		void SubmitLineQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color = glm::vec4(1));
		void SubmitAABB(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color);


		void EndScene(bool clear = true);
		

		const Renderer2DStats& GetStats();

	private:

		struct QuadVertex
		{
			glm::vec4 Color;
			glm::vec3 Position;
			glm::vec2 TexCoord;
			float	  TextureID;
			float	  TilingFactor;
		};

		struct LineVertex
		{
			glm::vec3 Position;
			glm::vec4 Color;
		};

		struct CircleVertex
		{
			glm::vec3 WorldPosition;
			float	  Thickness;
			glm::vec2 LocalPosition;
			glm::vec4 Color;
		};
	private:
		void resetQuads();
		void resetLines();

		void createRenderPass();
		void createDefaultPipelineBuckets();

		void flush();
		void flushLines();
		void flushFilledCircles();
		void updateRenderPass(Ref<Pipeline>& pipeline) const;

		Ref<Pipeline> setMaterial(std::map<size_t, Ref<Pipeline>>& pipelines, const Ref<Pipeline>& current, const Ref<Material>& material);

		uint32_t findTextureIndex(const Ref<Texture2D>& texture);
	
	private:	
		static constexpr uint32_t sc_MaxTextures = 32;
		static constexpr uint32_t sc_MaxQuads = 10000;
		static constexpr uint32_t sc_MaxVertices = sc_MaxQuads * 4;
		static constexpr uint32_t sc_MaxIndices = sc_MaxQuads * 6;
		static constexpr uint32_t sc_MaxLines = 10000;
		static constexpr uint32_t sc_MaxLineVertices = sc_MaxLines * 2;
		static constexpr uint32_t sc_MaxLineIndices = sc_MaxLines * 2;
		static constexpr uint32_t sc_MaxPoints = 10000;
		static constexpr glm::vec4 sc_QuadVertexPositions[4] = {
			{ -0.5f, -0.5f, 0.0f, 1.0f },
			{  0.5f, -0.5f, 0.0f, 1.0f },
			{  0.5f,  0.5f, 0.0f, 1.0f },
			{ -0.5f,  0.5f, 0.0f, 1.0f }
		};
	private:
		Ref<RenderCommandBuffer> m_RenderCommandBuffer;
		Ref<RenderPass>			 m_RenderPass;

		Ref<Material>			 m_QuadMaterial;
		Ref<Material>			 m_LineMaterial;
		Ref<Material>			 m_CircleMaterial;
								 
		Ref<Texture2D>							 m_WhiteTexture;
		std::array<Ref<Texture>, sc_MaxTextures> m_TextureSlots;
		uint32_t								 m_TextureSlotIndex = 0;

		std::map<size_t, Ref<Pipeline>> m_QuadPipelines;
		std::map<size_t, Ref<Pipeline>> m_LinePipelines;
		std::map<size_t, Ref<Pipeline>> m_CirclePipelines;

		Renderer2DBuffer<QuadVertex>   m_QuadBuffer;
		Renderer2DBuffer<LineVertex>   m_LineBuffer;
		Renderer2DBuffer<CircleVertex> m_CircleBuffer;


		Renderer2DStats		  m_Stats;	
		Ref<UniformBufferSet> m_UniformBufferSet;

		struct UBCamera
		{
			glm::mat4 ViewProjection;
		};
		glm::mat4 m_ViewMatrix;
	};

	template<typename VertexType>
	inline Renderer2DBuffer<VertexType>::~Renderer2DBuffer()
	{
		if (BufferBase)
		{
			delete[]BufferBase;
		}
	}


	template<typename VertexType>
	inline void Renderer2DBuffer<VertexType>::Init(const Ref<RenderPass>& renderPass, const Ref<Shader>& shader, uint32_t maxVertices, uint32_t* indices, uint32_t indexCount, const BufferLayout& layout)
	{
		this->BufferBase = new VertexType[maxVertices];
		
		this->VertexBuffer = VertexBuffer::Create(maxVertices * sizeof(VertexType));
		this->VertexBuffer->SetLayout(layout);
		this->IndexBuffer = IndexBuffer::Create(indices, indexCount);
		PipelineSpecification specification;
		specification.Shader = shader;
		specification.Layout = layout;
		specification.RenderPass = renderPass;
		this->Pipeline = Pipeline::Create(specification);
		Reset();
	}

	template<typename VertexType>
	inline void Renderer2DBuffer<VertexType>::Reset()
	{
		BufferPtr = BufferBase;
		IndexCount = 0;
	}
}