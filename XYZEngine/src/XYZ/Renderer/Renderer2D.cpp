#include "stdafx.h"
#include "Renderer2D.h"


#include "Renderer.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>

namespace XYZ {	

	

	static uint32_t* GenerateQuadIndices(uint32_t count)
	{
		uint32_t* indices = new uint32_t[count];
		uint32_t offset = 0;
		for (uint32_t i = 0; i < count; i += 6)
		{
			indices[i + 0] = offset + 0;
			indices[i + 1] = offset + 1;
			indices[i + 2] = offset + 2;
			
			indices[i + 3] = offset + 2;
			indices[i + 4] = offset + 3;
			indices[i + 5] = offset + 0;

			offset += 4;
		}
		return indices;
	}
	
	static uint32_t* GenerateLineIndices(uint32_t count)
	{
		uint32_t* indices = new uint32_t[count];
		for (uint32_t i = 0; i < count; i++)
		{
			indices[i] = i;
		}
		return indices;
	}

	Renderer2D::Renderer2D(const Renderer2DConfiguration& config)
		:
		m_RenderCommandBuffer(config.CommandBuffer),
		m_UniformBufferSet(config.UniformBufferSet)
	{
		XYZ_ASSERT(m_RenderCommandBuffer.Raw(), "");
		XYZ_ASSERT(m_UniformBufferSet.Raw(), "");

		createBuffers();
	}

	Renderer2D::~Renderer2D()
	{
	}

	void Renderer2D::BeginScene(const glm::mat4& viewMatrix)
	{
		m_Stats.DrawCalls = 0;
		m_Stats.LineDrawCalls = 0;
		m_Stats.CollisionDrawCalls = 0;
		m_Stats.FilledCircleDrawCalls = 0;

		const uint32_t currentFrame = Renderer::GetAPIContext()->GetCurrentFrame();
		m_ViewMatrix = viewMatrix;
	}

	void Renderer2D::SetCommandBuffer(const Ref<RenderCommandBuffer>& commandBuffer)
	{
		m_RenderCommandBuffer = commandBuffer;
	}

	void Renderer2D::SetUniformBufferSet(const Ref<UniformBufferSet>& uniformBufferSet)
	{
		m_UniformBufferSet = uniformBufferSet;
	}


	void Renderer2D::SubmitCircle(const glm::vec3& pos, float radius, uint32_t sides, const glm::vec4& color)
	{
		if (m_LineBuffer.IndexCount + (sides * 3) >= sc_MaxLineIndices)
			XYZ_ASSERT(false, "");

		const int step = 360 / sides;
		for (int a = step; a < 360 + step; a += step)
		{
			const float before = glm::radians((float)(a - step));
			const float heading = glm::radians((float)a);
			
			m_LineBuffer.BufferPtr->Position = glm::vec3(pos.x + std::cos(before) * radius, pos.y + std::sin(before) * radius, pos.z);
			m_LineBuffer.BufferPtr->Color = color;
			m_LineBuffer.BufferPtr++;
			m_LineBuffer.BufferPtr->Position = glm::vec3(pos.x + std::cos(heading) * radius, pos.y + std::sin(heading) * radius, pos.z);
			m_LineBuffer.BufferPtr->Color = color;
			m_LineBuffer.BufferPtr++;
			m_LineBuffer.IndexCount += 2;
		}
	}

	void Renderer2D::SubmitFilledCircle(const glm::vec3& pos, const glm::vec2& size, float thickness, const glm::vec4& color)
	{
		if (m_CircleBuffer.IndexCount >= sc_MaxIndices)
			XYZ_ASSERT(false, "");

		const glm::mat4 transform = glm::translate(glm::mat4(1.0f), pos)
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		for (int i = 0; i < 4; i++)
		{
			m_CircleBuffer.BufferPtr->WorldPosition = transform * sc_QuadVertexPositions[i];
			m_CircleBuffer.BufferPtr->Thickness = thickness;
			m_CircleBuffer.BufferPtr->LocalPosition = sc_QuadVertexPositions[i] * 2.0f;
			m_CircleBuffer.BufferPtr->Color = color;
			m_CircleBuffer.BufferPtr++;		
		}
		m_CircleBuffer.IndexCount += 6;
	}


	void Renderer2D::SubmitQuad(const glm::mat4& transform, const glm::vec4& texCoord, uint32_t textureIndex, const glm::vec4& color, float tilingFactor)
	{
		constexpr size_t quadVertexCount = 4;

		if (m_QuadBuffer.IndexCount + 6 >= sc_MaxIndices)
			XYZ_ASSERT(false, "");

		const glm::vec2 texCoords[quadVertexCount] = {
			{texCoord.x,texCoord.y},
			{texCoord.z,texCoord.y},
			{texCoord.z,texCoord.w},
			{texCoord.x,texCoord.w}
		};

		for (size_t i = 0; i < quadVertexCount; ++i)
		{
			m_QuadBuffer.BufferPtr->Position = transform * sc_QuadVertexPositions[i];
			m_QuadBuffer.BufferPtr->Color = color;
			m_QuadBuffer.BufferPtr->TexCoord = texCoords[i];
			m_QuadBuffer.BufferPtr->TextureID = (float)textureIndex;
			m_QuadBuffer.BufferPtr->TilingFactor = tilingFactor;
			m_QuadBuffer.BufferPtr++;
		}
		m_QuadBuffer.IndexCount += 6;
	}

	void Renderer2D::SubmitQuad(const glm::mat4& transform, const glm::vec4& color, float tilingFactor)
	{
		constexpr size_t quadVertexCount = 4;
		if (m_QuadBuffer.IndexCount >= sc_MaxIndices)
			XYZ_ASSERT(false, "");

		for (size_t i = 0; i < quadVertexCount; ++i)
		{
			m_QuadBuffer.BufferPtr->Position = transform * sc_QuadVertexPositions[i];
			m_QuadBuffer.BufferPtr->Color = color;
			m_QuadBuffer.BufferPtr->TexCoord = glm::vec2(0);
			m_QuadBuffer.BufferPtr->TextureID = 0.0f;
			m_QuadBuffer.BufferPtr->TilingFactor = tilingFactor;
			m_QuadBuffer.BufferPtr++;
		}
		m_QuadBuffer.IndexCount += 6;
	}



	void Renderer2D::SubmitQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& texCoord, uint32_t textureIndex, const glm::vec4& color, float tilingFactor)
	{
		constexpr size_t quadVertexCount = 4;

		if (m_QuadBuffer.IndexCount + 6 >= sc_MaxIndices)
			XYZ_ASSERT(false, "");

		const glm::vec2 texCoords[quadVertexCount] = {
			{texCoord.x, texCoord.y},
			{texCoord.z, texCoord.y},
			{texCoord.z, texCoord.w},
			{texCoord.x, texCoord.w}
		};
		const glm::vec3 vertices[quadVertexCount] = {
			{  position.x - size.x / 2.0f,  position.y - size.y / 2.0f, 0.0f},
			{  position.x + size.x / 2.0f,  position.y - size.y / 2.0f, 0.0f},
			{  position.x + size.x / 2.0f,  position.y + size.y / 2.0f, 0.0f},
			{  position.x - size.x / 2.0f,  position.y + size.y / 2.0f, 0.0f}
		};

		for (size_t i = 0; i < quadVertexCount; ++i)
		{
			m_QuadBuffer.BufferPtr->Position = vertices[i];
			m_QuadBuffer.BufferPtr->Color = color;
			m_QuadBuffer.BufferPtr->TexCoord = texCoords[i];
			m_QuadBuffer.BufferPtr->TextureID = (float)textureIndex;
			m_QuadBuffer.BufferPtr->TilingFactor = tilingFactor;
			m_QuadBuffer.BufferPtr++;
		}
		m_QuadBuffer.IndexCount += 6;
	}


	void Renderer2D::SubmitQuadBillboard(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
	{
		if (m_QuadBuffer.IndexCount >= sc_MaxIndices)
			XYZ_ASSERT(false, "");

		const float textureIndex = 0.0f; // White Texture
		const float tilingFactor = 1.0f;

		glm::vec3 camRightWS = { m_ViewMatrix[0][0], m_ViewMatrix[1][0], m_ViewMatrix[2][0] };
		glm::vec3 camUpWS = { m_ViewMatrix[0][1], m_ViewMatrix[1][1], m_ViewMatrix[2][1] };

		m_QuadBuffer.BufferPtr->Position = position + camRightWS * (sc_QuadVertexPositions[0].x) * size.x + camUpWS * sc_QuadVertexPositions[0].y * size.y;
		m_QuadBuffer.BufferPtr->Color = color;
		m_QuadBuffer.BufferPtr->TexCoord = { 0.0f, 0.0f };
		m_QuadBuffer.BufferPtr->TextureID = textureIndex;
		m_QuadBuffer.BufferPtr->TilingFactor = tilingFactor;
		m_QuadBuffer.BufferPtr++;

		m_QuadBuffer.BufferPtr->Position = position + camRightWS * sc_QuadVertexPositions[1].x * size.x + camUpWS * sc_QuadVertexPositions[1].y * size.y;
		m_QuadBuffer.BufferPtr->Color = color;
		m_QuadBuffer.BufferPtr->TexCoord = { 1.0f, 0.0f };
		m_QuadBuffer.BufferPtr->TextureID = textureIndex;
		m_QuadBuffer.BufferPtr->TilingFactor = tilingFactor;
		m_QuadBuffer.BufferPtr++;

		m_QuadBuffer.BufferPtr->Position = position + camRightWS * sc_QuadVertexPositions[2].x * size.x + camUpWS * sc_QuadVertexPositions[2].y * size.y;
		m_QuadBuffer.BufferPtr->Color = color;
		m_QuadBuffer.BufferPtr->TexCoord = { 1.0f, 1.0f };
		m_QuadBuffer.BufferPtr->TextureID = textureIndex;
		m_QuadBuffer.BufferPtr->TilingFactor = tilingFactor;
		m_QuadBuffer.BufferPtr++;

		m_QuadBuffer.BufferPtr->Position = position + camRightWS * sc_QuadVertexPositions[3].x * size.x + camUpWS * sc_QuadVertexPositions[3].y * size.y;
		m_QuadBuffer.BufferPtr->Color = color;
		m_QuadBuffer.BufferPtr->TexCoord = { 0.0f, 1.0f };
		m_QuadBuffer.BufferPtr->TextureID = textureIndex;
		m_QuadBuffer.BufferPtr->TilingFactor = tilingFactor;
		m_QuadBuffer.BufferPtr++;

		m_QuadBuffer.IndexCount += 6;
	}
	void Renderer2D::SubmitQuadBillboard(const glm::vec3& position, const glm::vec2& size, const glm::vec4& texCoord, uint32_t textureIndex, const glm::vec4& color, float tilingFactor)
	{
		if (m_QuadBuffer.IndexCount >= sc_MaxIndices)
			XYZ_ASSERT(false, "");

		const glm::vec2 texCoords[4] = {
			{texCoord.x,texCoord.y},
			{texCoord.z,texCoord.y},
			{texCoord.z,texCoord.w},
			{texCoord.x,texCoord.w}
		};

		glm::vec3 camRightWS = { m_ViewMatrix[0][0], m_ViewMatrix[1][0], m_ViewMatrix[2][0] };
		glm::vec3 camUpWS = { m_ViewMatrix[0][1], m_ViewMatrix[1][1], m_ViewMatrix[2][1] };

		m_QuadBuffer.BufferPtr->Position = position + camRightWS * (sc_QuadVertexPositions[0].x) * size.x + camUpWS * sc_QuadVertexPositions[0].y * size.y;
		m_QuadBuffer.BufferPtr->Color = color;
		m_QuadBuffer.BufferPtr->TexCoord = texCoords[0];
		m_QuadBuffer.BufferPtr->TextureID = (float)textureIndex;
		m_QuadBuffer.BufferPtr->TilingFactor = tilingFactor;
		m_QuadBuffer.BufferPtr++;

		m_QuadBuffer.BufferPtr->Position = position + camRightWS * sc_QuadVertexPositions[1].x * size.x + camUpWS * sc_QuadVertexPositions[1].y * size.y;
		m_QuadBuffer.BufferPtr->Color = color;
		m_QuadBuffer.BufferPtr->TexCoord = texCoords[1];
		m_QuadBuffer.BufferPtr->TextureID = (float)textureIndex;
		m_QuadBuffer.BufferPtr->TilingFactor = tilingFactor;
		m_QuadBuffer.BufferPtr++;

		m_QuadBuffer.BufferPtr->Position = position + camRightWS * sc_QuadVertexPositions[2].x * size.x + camUpWS * sc_QuadVertexPositions[2].y * size.y;
		m_QuadBuffer.BufferPtr->Color = color;
		m_QuadBuffer.BufferPtr->TexCoord = texCoords[2];
		m_QuadBuffer.BufferPtr->TextureID = (float)textureIndex;
		m_QuadBuffer.BufferPtr->TilingFactor = tilingFactor;
		m_QuadBuffer.BufferPtr++;

		m_QuadBuffer.BufferPtr->Position = position + camRightWS * sc_QuadVertexPositions[3].x * size.x + camUpWS * sc_QuadVertexPositions[3].y * size.y;
		m_QuadBuffer.BufferPtr->Color = color;
		m_QuadBuffer.BufferPtr->TexCoord = texCoords[3];
		m_QuadBuffer.BufferPtr->TextureID = (float)textureIndex;
		m_QuadBuffer.BufferPtr->TilingFactor = tilingFactor;
		m_QuadBuffer.BufferPtr++;

		m_QuadBuffer.IndexCount += 6;
	}

	void Renderer2D::SubmitQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color, float tilingFactor)
	{
		constexpr size_t quadVertexCount = 4;
		if (m_QuadBuffer.IndexCount >= sc_MaxIndices)
			XYZ_ASSERT(false, "");

		const glm::vec3 vertices[quadVertexCount] = {
			{  position.x - size.x / 2.0f,  position.y - size.y / 2.0f, 0.0f},
			{  position.x + size.x / 2.0f,  position.y - size.y / 2.0f, 0.0f},
			{  position.x + size.x / 2.0f,  position.y + size.y / 2.0f, 0.0f},
			{  position.x - size.x / 2.0f,  position.y + size.y / 2.0f, 0.0f}
		};
		for (size_t i = 0; i < quadVertexCount; ++i)
		{
			m_QuadBuffer.BufferPtr->Position = vertices[i];
			m_QuadBuffer.BufferPtr->Color = color;
			m_QuadBuffer.BufferPtr->TexCoord = glm::vec2(0);
			m_QuadBuffer.BufferPtr->TextureID = 0.0f;
			m_QuadBuffer.BufferPtr->TilingFactor = tilingFactor;
			m_QuadBuffer.BufferPtr++;
		}
		m_QuadBuffer.IndexCount += 6;
	}

	void Renderer2D::SubmitLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color)
	{
		if (m_LineBuffer.IndexCount >= sc_MaxLineIndices)
			XYZ_ASSERT(false, "");

		m_LineBuffer.BufferPtr->Position = p0;
		m_LineBuffer.BufferPtr->Color = color;
		m_LineBuffer.BufferPtr++;
		m_LineBuffer.BufferPtr->Position = p1;
		m_LineBuffer.BufferPtr->Color = color;
		m_LineBuffer.BufferPtr++;

		m_LineBuffer.IndexCount += 2;
	}

	void Renderer2D::SubmitRect(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
	{
		const glm::vec3 p[4] = {
			position,
			position + glm::vec3(size.x, 0.0f, 0.0f),
			position + glm::vec3(size.x, size.y, 0.0f),
			position + glm::vec3(0.0f,size.y, 0.0f)
		};
		SubmitLine(p[0], p[1], color);
		SubmitLine(p[1], p[2], color);
		SubmitLine(p[2], p[3], color);
		SubmitLine(p[3], p[0], color);
	}

	void Renderer2D::SubmitRect(const glm::mat4& transform, const glm::vec4& color)
	{
		glm::vec3 lineVertices[4];
		for (size_t i = 0; i < 4; i++)
			lineVertices[i] = transform * sc_QuadVertexPositions[i];

		SubmitLine(lineVertices[0], lineVertices[1], color);
		SubmitLine(lineVertices[1], lineVertices[2], color);
		SubmitLine(lineVertices[2], lineVertices[3], color);
		SubmitLine(lineVertices[3], lineVertices[0], color);
	}


	void Renderer2D::SubmitAABB(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color)
	{
		glm::vec3 topFrontLeft = { min.x, max.y, min.z };
		glm::vec3 topFrontRight = { max.x, max.y, min.z };
		glm::vec3 bottomFrontLeft = { min.x, min.y, min.z };
		glm::vec3 bottomFrontRight = { max.x, min.y, min.z };


		glm::vec3 topBackLeft = { min.x, max.y, max.z };
		glm::vec3 topBackRight = { max.x, max.y, max.z };
		glm::vec3 bottomBackLeft = { min.x, min.y, max.z };
		glm::vec3 bottomBackRight = { max.x, min.y, max.z };


		// Front
		SubmitLine(topFrontLeft, topFrontRight, color);
		SubmitLine(topFrontRight, bottomFrontRight, color);
		SubmitLine(bottomFrontRight, bottomFrontLeft, color);
		SubmitLine(bottomFrontLeft, topFrontLeft, color);


		//Back
		SubmitLine(topBackLeft, topBackRight, color);
		SubmitLine(topBackRight, bottomBackRight, color);
		SubmitLine(bottomBackRight, bottomBackLeft, color);
		SubmitLine(bottomBackLeft, topBackLeft, color);

		// Sides
		SubmitLine(topFrontLeft, topBackLeft, color);
		SubmitLine(topFrontRight, topBackRight, color);
		SubmitLine(bottomFrontLeft,  bottomBackLeft, color);
		SubmitLine(bottomFrontRight, bottomBackRight, color);
	}


	void Renderer2D::SubmitQuadNotCentered(const glm::vec3& position, const glm::vec2& size, const glm::vec4& texCoord, uint32_t textureIndex, const glm::vec4& color, float tilingFactor)
	{
		constexpr size_t quadVertexCount = 4;

		if (m_QuadBuffer.IndexCount + 6 >= sc_MaxIndices)
			XYZ_ASSERT(false, "");

		const glm::vec2 texCoords[quadVertexCount] = {
			{texCoord.x, texCoord.y},
			{texCoord.z, texCoord.y},
			{texCoord.z, texCoord.w},
			{texCoord.x, texCoord.w}
		};

		const glm::vec3 vertices[quadVertexCount] = {
			{  position.x ,			 position.y, 0.0f},
			{  position.x + size.x,  position.y, 0.0f},
			{  position.x + size.x,  position.y + size.y, 0.0f},
			{  position.x,			 position.y + size.y, 0.0f}
		};

		for (size_t i = 0; i < quadVertexCount; ++i)
		{
			m_QuadBuffer.BufferPtr->Position = vertices[i];
			m_QuadBuffer.BufferPtr->Color = color;
			m_QuadBuffer.BufferPtr->TexCoord = texCoords[i];
			m_QuadBuffer.BufferPtr->TextureID = (float)textureIndex;
			m_QuadBuffer.BufferPtr->TilingFactor = tilingFactor;
			m_QuadBuffer.BufferPtr++;
		}
		m_QuadBuffer.IndexCount += 6;
	}

	void Renderer2D::FlushQuads(const Ref<Pipeline>& pipeline, const Ref<MaterialInstance>& materialInstance, bool reset)
	{
		const uint32_t dataSize = m_QuadBuffer.DataSize();
		if (dataSize)
		{
			XYZ_ASSERT(dataSize + m_QuadBuffer.Offset < m_QuadBuffer.VertexBuffer->GetSize(), "");
			m_QuadBuffer.VertexBuffer->Update(m_QuadBuffer.DataPtr(), dataSize, m_QuadBuffer.Offset);

			Renderer::RenderGeometry(m_RenderCommandBuffer, pipeline, materialInstance, m_QuadBuffer.VertexBuffer, m_QuadBuffer.IndexBuffer, glm::mat4(1.0f), m_QuadBuffer.IndexCount, m_QuadBuffer.Offset);
			m_Stats.DrawCalls++;

			m_QuadBuffer.IndexCount = 0;
			m_QuadBuffer.Offset += dataSize;
		}
		if (reset)
			m_QuadBuffer.Reset();
	}
	void Renderer2D::FlushLines(const Ref<Pipeline>& pipeline, const Ref<MaterialInstance>& materialInstance, bool reset, const PushConstBuffer& pushConst)
	{
		const uint32_t dataSize = m_LineBuffer.DataSize();
		if (dataSize)
		{
			XYZ_ASSERT(dataSize + m_LineBuffer.Offset < m_LineBuffer.VertexBuffer->GetSize(), "");
			m_LineBuffer.VertexBuffer->Update(m_LineBuffer.DataPtr(), dataSize, m_LineBuffer.Offset);

			Renderer::RenderGeometry(m_RenderCommandBuffer, pipeline, materialInstance, m_LineBuffer.VertexBuffer, m_LineBuffer.IndexBuffer, pushConst, m_LineBuffer.IndexCount, m_LineBuffer.Offset);

			m_Stats.LineDrawCalls++;
			m_LineBuffer.IndexCount = 0;
			m_LineBuffer.Offset += dataSize;
		}
		if (reset)
			m_LineBuffer.Reset();
	}


	void Renderer2D::FlushFilledCircles(const Ref<Pipeline>& pipeline, const Ref<MaterialInstance>& materialInstance, bool reset)
	{
		const uint32_t dataSize = m_CircleBuffer.DataSize();
		if (dataSize)
		{
			XYZ_ASSERT(dataSize + m_CircleBuffer.Offset < m_CircleBuffer.VertexBuffer->GetSize(), "");
			m_CircleBuffer.VertexBuffer->Update(m_CircleBuffer.DataPtr(), dataSize, m_CircleBuffer.Offset);

			Renderer::RenderGeometry(m_RenderCommandBuffer, pipeline, materialInstance, m_CircleBuffer.VertexBuffer, m_CircleBuffer.IndexBuffer, glm::mat4(1.0f), m_CircleBuffer.IndexCount, m_CircleBuffer.Offset);

			m_Stats.FilledCircleDrawCalls++;

			m_CircleBuffer.IndexCount = 0;
			m_CircleBuffer.Offset += dataSize;
		}
		if (reset)
			m_CircleBuffer.Reset();
	}

	void Renderer2D::EndScene(bool reset)
	{	
		if (reset)
		{
			m_QuadBuffer.Reset();
			m_LineBuffer.Reset();
			m_CircleBuffer.Reset();
		}
	}


	void Renderer2D::createBuffers()
	{
		uint32_t* quadIndices = GenerateQuadIndices(sc_MaxIndices);
		uint32_t* lineIndices = GenerateLineIndices(sc_MaxLineIndices);
		
		m_QuadBuffer.Init(sc_MaxVertices, quadIndices, sc_MaxIndices);
		m_LineBuffer.Init(sc_MaxLineVertices, lineIndices, sc_MaxLineIndices);
		m_CircleBuffer.Init(sc_MaxVertices, quadIndices, sc_MaxIndices);

		delete[]quadIndices;
		delete[]lineIndices;
	}

	const Renderer2DStats& Renderer2D::GetStats()
	{
		return m_Stats;
	}
}