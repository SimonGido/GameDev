#include "stdafx.h"
#include "VoxelMeshCollision.h"


namespace XYZ {

	bool IsValidVoxel(const glm::ivec3& voxel, uint32_t width, uint32_t height, uint32_t depth)
	{
		return ((voxel.x < width && voxel.x >= 0)
			&& (voxel.y < height && voxel.y >= 0)
			&& (voxel.z < depth && voxel.z >= 0));
	}

	uint32_t Index3D(int32_t x, int32_t y, int32_t z, uint32_t width, uint32_t height)
	{
		return x + width * (y + height * z);
	}

	static uint32_t Index3D(const glm::ivec3& index, uint32_t width, uint32_t height)
	{
		return Index3D(index.x, index.y, index.z, width, height);
	}

	static AABB VoxelAABB(glm::ivec3 voxel, float voxelSize)
	{
		AABB result;
		result.Min = glm::vec3(voxel.x * voxelSize, voxel.y * voxelSize, voxel.z * voxelSize);
		result.Max = result.Min + voxelSize;
		return result;
	}

	VoxelMeshCollision::CollisionResult VoxelMeshCollision::IsCollision(const glm::vec3& point, const VoxelSubmesh& submesh, const glm::mat4& transform)
	{
		CollisionResult result;

		const glm::vec4 local = glm::inverse(transform) * glm::vec4(point, 1.0);
		const glm::vec3 localPoint = glm::vec3(local);
		const glm::ivec3 currentVoxel = localPoint / submesh.VoxelSize;

		if (!IsValidVoxel(currentVoxel, submesh.Width, submesh.Height, submesh.Depth))
			return result;

		if (submesh.Compressed)
		{
			const uint32_t cellIndex = Index3D(currentVoxel, submesh.Width, submesh.Height);
			
			const auto& cell = submesh.CompressedCells[cellIndex];

			if (cell.VoxelCount == 1)
			{
				const uint8_t colorIndex = submesh.ColorIndices[cell.VoxelOffset];
				result.VoxelIndices.push_back(cell.VoxelOffset);
				return result;
			}
			else
			{
				const uint32_t cellWidth = submesh.CompressScale;
				const uint32_t cellHeight = submesh.CompressScale;
				const uint32_t cellDepth = submesh.CompressScale;
				const float cellVoxelSize = submesh.VoxelSize / submesh.CompressScale;
				
				const glm::ivec3 decompressedVoxelOffset = currentVoxel * (int32_t)submesh.CompressScale;
				const glm::ivec3 decompressedCurrentVoxel = localPoint / cellVoxelSize;

				const glm::ivec3 cellVoxel = decompressedCurrentVoxel - decompressedVoxelOffset;

				
				if (!IsValidVoxel(cellVoxel, cellWidth, cellHeight, cellDepth))
					return result;

				const uint32_t voxelIndex = Index3D(cellVoxel, cellWidth, cellHeight) + cell.VoxelOffset;
				const uint8_t colorIndex = submesh.ColorIndices[voxelIndex];
				result.VoxelIndices.push_back(voxelIndex);
				return result;
			}
		}

		return result;
	}

	VoxelMeshCollision::CollisionResult VoxelMeshCollision::IsCollision(const AABB& aabb, const VoxelSubmesh& submesh, const glm::mat4& transform)
	{
		CollisionResult result;

		glm::mat4 inverseTransform = glm::inverse(transform);
		AABB localAABB;
		localAABB.Min = inverseTransform * glm::vec4(aabb.Min, 1.0);
		localAABB.Max = inverseTransform * glm::vec4(aabb.Max, 1.0);
		
		const glm::ivec3 minVoxel = glm::floor(localAABB.Min / submesh.VoxelSize);
		const glm::ivec3 maxVoxel = glm::ceil(localAABB.Max / submesh.VoxelSize);

		for (int32_t cellx = minVoxel.x; cellx <= maxVoxel.x; cellx++)
		{
			for (int32_t celly = minVoxel.y; celly <= maxVoxel.y; celly++)
			{
				for (int32_t cellz = minVoxel.z; cellz <= maxVoxel.z; cellz++)
				{
					const uint32_t cellIndex = Index3D(cellx, celly, cellz, submesh.Width, submesh.Height);
					if (!IsValidVoxel({ cellx, celly, cellz }, submesh.Width, submesh.Height, submesh.Depth))
						continue;

					const auto& cell = submesh.CompressedCells[cellIndex];
					AABB cellAABB = VoxelAABB({ cellx, celly, cellz }, submesh.VoxelSize);
					if (cellAABB.Intersect(localAABB))
					{
						if (cell.VoxelCount == 1)
						{
							result.VoxelIndices.push_back(cell.VoxelOffset);
						}
						else
						{
							const uint32_t decompressedStartX = cellx * submesh.CompressScale;
							const uint32_t decompressedStartY = celly * submesh.CompressScale;
							const uint32_t decompressedStartZ = cellz * submesh.CompressScale;

							const uint32_t decompressedEndX = (cellx + 1) * submesh.CompressScale;
							const uint32_t decompressedEndY = (celly + 1) * submesh.CompressScale;
							const uint32_t decompressedEndZ = (cellz + 1) * submesh.CompressScale;


							for (uint32_t x = decompressedStartX; x < decompressedEndX; x++)
							{
								for (uint32_t y = decompressedStartY; y < decompressedEndY; y++)
								{
									for (uint32_t z = decompressedStartZ; z < decompressedEndZ; z++)
									{					
										AABB voxelAABB = VoxelAABB({ x, y, z }, submesh.VoxelSize / submesh.CompressScale);
										if (voxelAABB.Intersect(localAABB))
										{
											const glm::ivec3 currentVoxel = {
												x - decompressedStartX,
												y - decompressedStartY,
												z - decompressedStartZ
											};
											const uint32_t voxelIndex = Index3D(currentVoxel, submesh.CompressScale, submesh.CompressScale);
											if (voxelIndex < cell.VoxelCount)
											{
												result.VoxelIndices.push_back(cell.VoxelOffset + voxelIndex);
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
		return result;
	}
}