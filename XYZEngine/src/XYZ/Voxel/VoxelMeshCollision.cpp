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

	VoxelMeshCollision::CollisionResult VoxelMeshCollision::IsCollision(const glm::vec3& point, const VoxelSubmesh& submesh, const glm::mat4& transform)
	{
		CollisionResult result;
		result.Collide = false;

		const glm::vec4 local = glm::inverse(transform) * glm::vec4(point, 1.0);
		const glm::vec3 localPoint = glm::vec3(local);
		const glm::ivec3 currentVoxel = localPoint / submesh.VoxelSize;

		if (!IsValidVoxel(currentVoxel, submesh.Width, submesh.Height, submesh.Depth))
			return result;

		if (submesh.Compressed)
		{
			const uint32_t cellIndex = Index3D(currentVoxel, submesh.Width, submesh.Height);
			
			const auto& cell = submesh.CompressedCells[cellIndex];
			result.CellIndex = cellIndex;

			if (cell.VoxelCount == 1)
			{
				const uint8_t colorIndex = submesh.ColorIndices[cell.VoxelOffset];
				result.Collide = true;
				result.VoxelIndex = cell.VoxelOffset;
				return result;
			}
			else
			{
				const uint32_t cellWidth = submesh.Width / submesh.CompressScale;
				const uint32_t cellHeight = submesh.Height / submesh.CompressScale;
				const uint32_t cellDepth = submesh.Depth / submesh.CompressScale;
				const float cellVoxelSize = submesh.VoxelSize / submesh.CompressScale;
				
				const glm::ivec3 decompressedVoxelOffset = currentVoxel * (int32_t)submesh.CompressScale;
				const glm::ivec3 decompressedCurrentVoxel = localPoint / cellVoxelSize;

				const glm::ivec3 cellVoxel = decompressedCurrentVoxel - decompressedVoxelOffset;

				
				if (!IsValidVoxel(cellVoxel, cellWidth, cellHeight, cellDepth))
					return result;

				const uint32_t voxelIndex = Index3D(cellVoxel, cellWidth, cellHeight) + cell.VoxelOffset;
				const uint8_t colorIndex = submesh.ColorIndices[voxelIndex];
				result.Collide = true;
				result.VoxelIndex = voxelIndex;
				return result;
			}
		}

		return result;
	}
}