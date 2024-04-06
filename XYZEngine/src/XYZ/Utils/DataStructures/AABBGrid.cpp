#include "stdafx.h"
#include "AABBGrid.h"

#include "XYZ/Utils/Math/Math.h"


namespace XYZ {

	static uint32_t Index3D(uint32_t x, uint32_t y, uint32_t z, uint32_t width, uint32_t height)
	{
		return x + width * (y + height * z);
	}

	void AABBGrid::Initialize(const glm::vec3& position, uint32_t width, uint32_t height, uint32_t depth, float cellSize)
	{
		m_Position = position;
		m_Width = width;
		m_Height = height;
		m_Depth = depth;
		m_CellSize = cellSize;

		m_Cells.clear();
		m_Cells.resize(width * height * depth);
	}
	void AABBGrid::Insert(const AABB& aabb, int32_t data)
	{
		const glm::vec3 localMin = aabb.Min - m_Position;
		const glm::vec3 localMax = aabb.Max - m_Position;

		const int32_t startX = std::floor(localMin.x / m_CellSize);
		const int32_t startY = std::floor(localMin.y / m_CellSize);
		const int32_t startZ = std::floor(localMin.z / m_CellSize);

		const int32_t endX = std::ceil(localMax.x / m_CellSize);
		const int32_t endY = std::ceil(localMax.y / m_CellSize);
		const int32_t endZ = std::ceil(localMax.z / m_CellSize);

		for (int32_t x = startX; x < endX; ++x)
		{
			if (x < 0 || x >= m_Width)
				continue;

			for (int32_t y = startY; y < endY; ++y)
			{
				if (y < 0 || y >= m_Height)
					continue;

				for (int32_t z = startZ; z < endZ; ++z)
				{
					if (z < 0 || z >= m_Depth)
						continue;

					const uint32_t index = Index3D(x, y, z, m_Width, m_Height);
					m_Cells[index].push_back(data);
				}
			}
		}
	}
	void AABBGrid::Insert(const AABB& aabb, int32_t data, const Math::Frustum& frustum)
	{
		const glm::vec3 localMin = aabb.Min - m_Position;
		const glm::vec3 localMax = aabb.Max - m_Position;

		const int32_t startX = std::floor(localMin.x / m_CellSize);
		const int32_t startY = std::floor(localMin.y / m_CellSize);
		const int32_t startZ = std::floor(localMin.z / m_CellSize);

		const int32_t endX = std::ceil(localMax.x / m_CellSize);
		const int32_t endY = std::ceil(localMax.y / m_CellSize);
		const int32_t endZ = std::ceil(localMax.z / m_CellSize);

		for (int32_t x = startX; x < endX; ++x)
		{
			if (x < 0 || x >= m_Width)
				continue;

			for (int32_t y = startY; y < endY; ++y)
			{
				if (y < 0 || y >= m_Height)
					continue;

				for (int32_t z = startZ; z < endZ; ++z)
				{
					if (z < 0 || z >= m_Depth)
						continue;

					AABB cellAABB;
					cellAABB.Min = glm::vec3(
						m_Position.x + x * m_CellSize,
						m_Position.y + y * m_CellSize,
						m_Position.z + z * m_CellSize
					);
					cellAABB.Max = glm::vec3(
						cellAABB.Min.x + m_CellSize,
						cellAABB.Min.y + m_CellSize,
						cellAABB.Min.z + m_CellSize
					);
					if (cellAABB.InsideFrustum(frustum))
					{
						const uint32_t index = Index3D(x, y, z, m_Width, m_Height);
						m_Cells[index].push_back(data);
						return;
					}
				}
			}
		}
	}
}