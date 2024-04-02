#pragma once

#include "Camera.h"
#include "Mesh.h"
#include "Font.h"
#include "Pipeline.h"
#include "RenderCommandBuffer.h"
#include "UniformBufferSet.h"
#include "SubTexture.h"
#include "XYZ/Asset/Renderer/MaterialAsset.h"

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
	struct XYZ_API Renderer2DBuffer
	{
		Renderer2DBuffer() = default;
		~Renderer2DBuffer();

		void Init(uint32_t maxVertices, uint32_t* indices, uint32_t indexCount);
		void Reset();
		uint32_t DataSize() const;
		uint8_t* DataPtr() const;

		Ref<IndexBuffer>  IndexBuffer;
		Ref<VertexBuffer> VertexBuffer;
		VertexType*		  BufferBase = nullptr;
		VertexType*		  BufferPtr  = nullptr;
		uint32_t		  IndexCount = 0;
		uint32_t		  Offset = 0;
	};

	struct Renderer2DConfiguration
	{
		Ref<RenderCommandBuffer> CommandBuffer;
		Ref<UniformBufferSet>    UniformBufferSet;
	};

	class XYZ_API Renderer2D : public RefCount
	{
	public:
		Renderer2D(const Renderer2DConfiguration& config);
		~Renderer2D();

		void BeginScene(const glm::mat4& viewMatrix);

		void SetCommandBuffer(const Ref<RenderCommandBuffer>& commandBuffer);
		void SetUniformBufferSet(const Ref<UniformBufferSet>& uniformBufferSet);


		void SubmitCircle(const glm::vec3& pos, float radius, uint32_t sides, const glm::vec4& color = glm::vec4(1.0f));
		void SubmitFilledCircle(const glm::vec3& pos, const glm::vec2& size, float thickness, const glm::vec4& color = glm::vec4(1.0f));
		

		void SubmitQuad(const glm::mat4& transform, const glm::vec4& color, float tilingFactor = 1.0f);
		void SubmitQuad(const glm::mat4& transform, const glm::vec4& texCoord, uint32_t textureIndex, const glm::vec4& color = glm::vec4(1), float tilingFactor = 1.0f);
		void SubmitQuad(const glm::vec3& position,	const glm::vec2& size, const glm::vec4& texCoord, uint32_t textureIndex, const glm::vec4& color, float tilingFactor = 1.0f);
		

		void SubmitQuadBillboard(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);
		void SubmitQuadBillboard(const glm::vec3& position, const glm::vec2& size, const glm::vec4& texCoord, uint32_t textureIndex, const glm::vec4& color = glm::vec4(1.0f), float tilingFactor = 1.0f);
	
		void SubmitQuad(const glm::vec3& position,	const glm::vec2& size, const glm::vec4& color, float tilingFactor);
		void SubmitQuadNotCentered(const glm::vec3& position, const glm::vec2& size, const glm::vec4& texCoord, uint32_t textureIndex, const glm::vec4& color = glm::vec4(1), float tilingFactor = 1.0f);

		void SubmitLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color = glm::vec4(1.0f));
		void SubmitRect(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color = glm::vec4(1.0f));
		void SubmitRect(const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f));
		void SubmitAABB(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color);


		void FlushQuads(const Ref<Pipeline>& pipeline, const Ref<MaterialInstance>& materialInstance, bool reset);
		void FlushLines(const Ref<Pipeline>& pipeline, const Ref<MaterialInstance>& materialInstance, bool reset, const PushConstBuffer& pushConst = {glm::mat4(1.0f)});
		void FlushFilledCircles(const Ref<Pipeline>& pipeline, const Ref<MaterialInstance>& materialInstance, bool reset);

		void EndScene(bool reset = true);
			
		const Renderer2DStats&	  GetStats();

		static constexpr uint32_t GetMaxTextures() { return sc_MaxTextures; }

		static constexpr uint32_t GetMaxLines() { return sc_MaxLines; }
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
			glm::vec4 Color;
			glm::vec3 Position;
		};

		struct CircleVertex
		{
			glm::vec3 WorldPosition;
			float	  Thickness;
			glm::vec2 LocalPosition;
			glm::vec4 Color;
		};
	private:
		void createBuffers();

		
	private:	
		static constexpr uint32_t sc_MaxTextures = 32;
		static constexpr uint32_t sc_MaxQuads = 10000;
		static constexpr uint32_t sc_MaxVertices = sc_MaxQuads * 4;
		static constexpr uint32_t sc_MaxIndices = sc_MaxQuads * 6;
		static constexpr uint32_t sc_MaxLines = 100000;
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
		Ref<UniformBufferSet>	 m_UniformBufferSet;

							 
		Renderer2DBuffer<QuadVertex>   m_QuadBuffer;
		Renderer2DBuffer<LineVertex>   m_LineBuffer;
		Renderer2DBuffer<CircleVertex> m_CircleBuffer;


		Renderer2DStats		  m_Stats;	
		

		glm::mat4			  m_ViewMatrix;
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
	inline void Renderer2DBuffer<VertexType>::Init(uint32_t maxVertices, uint32_t* indices, uint32_t indexCount)
	{
		this->BufferBase = new VertexType[maxVertices];
		this->VertexBuffer = VertexBuffer::Create(maxVertices * sizeof(VertexType));
		this->IndexBuffer = IndexBuffer::Create(indices, indexCount);
		Reset();
	}

	template<typename VertexType>
	inline void Renderer2DBuffer<VertexType>::Reset()
	{
		BufferPtr = BufferBase;
		IndexCount = 0;
		Offset = 0;
	}
	template<typename VertexType>
	inline uint32_t Renderer2DBuffer<VertexType>::DataSize() const
	{
		return (uint8_t*)BufferPtr - (uint8_t*)BufferBase - Offset;
	}
	template<typename VertexType>
	inline uint8_t* Renderer2DBuffer<VertexType>::DataPtr() const
	{
		return (uint8_t*)BufferBase + Offset;
	}
}