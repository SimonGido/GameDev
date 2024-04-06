#pragma once
#include "XYZ/Utils/Math/AABB.h"

namespace XYZ {

	class AABBGrid
	{
	public:
		using Cell = std::vector<int32_t>;

		void Initialize(const glm::vec3& position, uint32_t width, uint32_t height, uint32_t depth, float cellSize);
		void Insert(const AABB& aabb, int32_t data);
		void Insert(const AABB& aabb, int32_t data, const Math::Frustum& frustum);

		const glm::vec3&		GetPosition() const { return m_Position; }
		const glm::ivec3		GetDimensions() const { return { m_Width, m_Height, m_Depth }; }
		float					GetCellSize() const { return m_CellSize; }
		const std::vector<Cell>& GetCells() const { return m_Cells; }

	private:
		
	private:
		glm::vec3	m_Position;
		uint32_t	m_Width;
		uint32_t	m_Height;
		uint32_t	m_Depth;
		float		m_CellSize; 

		std::vector<Cell> m_Cells;
	};

}