#pragma once
#include "Mesh.h"

namespace XYZ {

	

	class XYZ_API MeshFactory
	{
	public:
		static Ref<MeshSource> CreateQuad(const glm::vec2& size);
		static Ref<StaticMesh> CreateInstancedQuad(const glm::vec2& size, const BufferLayout& layout, const BufferLayout& instanceLayout, uint32_t count);
		
		static Ref<StaticMesh> CreateBox(const glm::vec3& size, const glm::vec4& color = glm::vec4(1.0f));
		static Ref<StaticMesh> CreateInstancedBox(const glm::vec3& size, const BufferLayout& layout, const BufferLayout& instanceLayout, uint32_t count);

		// Cube is just a box with 24 vertices, required for texturing
		static Ref<StaticMesh> CreateCube(const glm::vec3& size, const glm::vec4& color = glm::vec4(1.0f));
		static Ref<StaticMesh> CreateInstancedCube(const glm::vec3& size, const BufferLayout& layout, const BufferLayout& instanceLayout, uint32_t count);
	};
}