#include "stdafx.h"
#include "MeshFactory.h"

namespace XYZ {
	

	Ref<MeshSource> MeshFactory::CreateQuad(const glm::vec2& size)
	{   
		const std::vector<Vertex> vertices{
			Vertex{glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec3(0), glm::vec3(0), glm::vec3(0), glm::vec2(0.0f, 0.0f)},
			Vertex{glm::vec3( 0.5f, -0.5f, 0.0f), glm::vec3(0), glm::vec3(0), glm::vec3(0), glm::vec2(1.0f, 0.0f)},
			Vertex{glm::vec3( 0.5f,  0.5f, 0.0f), glm::vec3(0), glm::vec3(0), glm::vec3(0), glm::vec2(1.0f, 1.0f)},
			Vertex{glm::vec3(-0.5f,  0.5f, 0.0f), glm::vec3(0), glm::vec3(0), glm::vec3(0), glm::vec2(0.0f, 1.0f)}
		};
		const std::vector<uint32_t> indices { 0, 1, 2, 2, 3, 0 };
		return Ref<MeshSource>::Create(vertices, indices);
	}
	Ref<StaticMesh> MeshFactory::CreateInstancedQuad(const glm::vec2& size, const BufferLayout& layout, const BufferLayout& instanceLayout, uint32_t count)
	{
		Ref<StaticMesh> result;
		return result;
	}



	Ref<StaticMesh> MeshFactory::CreateBox(const glm::vec3& size, const glm::vec4& color)
	{
		std::vector<ColoredVertex> vertices = {
			ColoredVertex{{ -size.x / 2.0f, -size.y / 2.0f,  size.z / 2.0f }, {-1, -1, 1}, color},  // Front Down Left
			ColoredVertex{{  size.x / 2.0f, -size.y / 2.0f,  size.z / 2.0f }, { 1, -1, 1}, color},  // Front Down Right 
			ColoredVertex{{  size.x / 2.0f,  size.y / 2.0f,  size.z / 2.0f }, { 1,  1, 1}, color},  // Front Up   Right
			ColoredVertex{{ -size.x / 2.0f,  size.y / 2.0f,  size.z / 2.0f }, {-1,  1, 1}, color},	 // Front Up   Left

			ColoredVertex{{ -size.x / 2.0f, -size.y / 2.0f, -size.z / 2.0f }, {-1, -1, -1}, color},	// Back  Down Left
			ColoredVertex{{  size.x / 2.0f, -size.y / 2.0f, -size.z / 2.0f }, { 1, -1, -1}, color},	// Back  Down Right
			ColoredVertex{{  size.x / 2.0f,  size.y / 2.0f, -size.z / 2.0f }, { 1,  1, -1}, color},	// Back  Up	  Right
			ColoredVertex{{ -size.x / 2.0f,  size.y / 2.0f, -size.z / 2.0f }, {-1,  1, -1}, color}	// Back  Up	  Left
		};

		std::vector<uint32_t> indices = {
			// Front face
			 0, 1, 2,
			 2, 3, 0,
			 // Right face
			 1, 5, 6,
			 6, 2, 1,
			 // Back face
			 7, 6, 5,
			 5, 4, 7,
			 // Left face
			 4, 0, 3,
			 3, 7, 4,
			 // Bottom face
			 4, 5, 1,
			 1, 0, 4,
			 // Top face
			 3, 2, 6,
			 6, 7, 3
		};

		Ref<StaticMesh> result = Ref<StaticMesh>::Create(Ref<MeshSource>::Create(vertices, indices));
		return result;
	}
	Ref<StaticMesh> MeshFactory::CreateInstancedBox(const glm::vec3& size, const BufferLayout& layout, const BufferLayout& instanceLayout, uint32_t count)
	{
		Ref<StaticMesh> result;
		return result;
	}
	Ref<StaticMesh> MeshFactory::CreateCube(const glm::vec3& size, const glm::vec4& color)
	{
		std::vector<ColoredVertex> vertices = {
			// Front face
			ColoredVertex{{ -size.x / 2.0f, -size.y / 2.0f,  -size.z / 2.0f },{0, 0, 1}, color},
			ColoredVertex{{  size.x / 2.0f, -size.y / 2.0f,  -size.z / 2.0f },{0, 0, 1}, color},
			ColoredVertex{{  size.x / 2.0f,  size.y / 2.0f,  -size.z / 2.0f },{0, 0, 1}, color},
			ColoredVertex{{ -size.x / 2.0f,  size.y / 2.0f,  -size.z / 2.0f },{0, 0, 1}, color},

			// Top face
			ColoredVertex{{ -size.x / 2.0f,  size.y / 2.0f,  size.z / 2.0f },{0, 1, 0}, color},
			ColoredVertex{{  size.x / 2.0f,  size.y / 2.0f,  size.z / 2.0f },{0, 1, 0}, color},
			ColoredVertex{{  size.x / 2.0f,  size.y / 2.0f, -size.z / 2.0f },{0, 1, 0}, color},
			ColoredVertex{{ -size.x / 2.0f,  size.y / 2.0f, -size.z / 2.0f },{0, 1, 0}, color},
			
			//// Bottom face
			ColoredVertex{{ -size.x / 2.0f, -size.y / 2.0f,  size.z / 2.0f },{0, -1, 0}, color},
			ColoredVertex{{  size.x / 2.0f, -size.y / 2.0f,  size.z / 2.0f },{0, -1, 0}, color},
			ColoredVertex{{  size.x / 2.0f, -size.y / 2.0f, -size.z / 2.0f },{0, -1, 0}, color},
			ColoredVertex{{ -size.x / 2.0f, -size.y / 2.0f, -size.z / 2.0f },{0, -1, 0}, color},
			//
			// Left face
			ColoredVertex{{ -size.x / 2.0f, -size.y / 2.0f,  size.z / 2.0f },{-1, 0, 0}, color},
			ColoredVertex{{ -size.x / 2.0f, -size.y / 2.0f, -size.z / 2.0f },{-1, 0, 0}, color},
			ColoredVertex{{ -size.x / 2.0f,  size.y / 2.0f, -size.z / 2.0f },{-1, 0, 0}, color},
			ColoredVertex{{ -size.x / 2.0f,  size.y / 2.0f,  size.z / 2.0f },{-1, 0, 0}, color},
			
			// Right face 
			ColoredVertex{{  size.x / 2.0f, -size.y / 2.0f,  size.z / 2.0f },{1, 0, 0}, color},
			ColoredVertex{{  size.x / 2.0f, -size.y / 2.0f, -size.z / 2.0f },{1, 0, 0}, color},
			ColoredVertex{{  size.x / 2.0f,  size.y / 2.0f, -size.z / 2.0f },{1, 0, 0}, color},
			ColoredVertex{{  size.x / 2.0f,  size.y / 2.0f,  size.z / 2.0f },{1, 0, 0}, color},
			
			// Back face
			ColoredVertex{{ -size.x / 2.0f, -size.y / 2.0f, size.z / 2.0f },{0, 0, -1}, color},
			ColoredVertex{{  size.x / 2.0f, -size.y / 2.0f, size.z / 2.0f },{0, 0, -1}, color},
			ColoredVertex{{  size.x / 2.0f,  size.y / 2.0f, size.z / 2.0f },{0, 0, -1}, color},
			ColoredVertex{{ -size.x / 2.0f,  size.y / 2.0f, size.z / 2.0f },{0, 0, -1}, color}
		};

		std::vector<uint32_t> indices = {
			// Front face (CCW winding)
			0, 3, 2,
			2, 1, 0,

			// Top face (CCW winding)
			6, 7, 4,
			4, 5, 6,
			
			// Bottom face (CCW winding)
			10, 9, 8,
			8, 11, 10,
			
			// Left face (CCW winding)
			12, 15, 14,
			14, 13, 12,
			
			// Right face (CCW winding)
			18, 19, 16,
			16, 17, 18,

			// Back face (CCW winding)
			22, 23, 20,
			20, 21, 22,
		};

		Ref<StaticMesh> result = Ref<StaticMesh>::Create(Ref<MeshSource>::Create(vertices, indices));
		return result;
	}
	Ref<StaticMesh> MeshFactory::CreateInstancedCube(const glm::vec3& size, const BufferLayout& layout, const BufferLayout& instanceLayout, uint32_t count)
	{
		Ref<StaticMesh> result;
		return result;
	}
}
