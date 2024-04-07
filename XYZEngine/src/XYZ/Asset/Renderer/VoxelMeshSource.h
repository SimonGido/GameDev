#pragma once
#include "XYZ/Asset/Asset.h"
#include "XYZ/Utils/Math/AABB.h"

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

namespace XYZ {



	struct XYZ_API VoxelSubmesh
	{		
		struct CompressedCell
		{
			uint32_t VoxelCount = 0;
			uint32_t VoxelOffset = 0;
		};


		uint32_t Width;
		uint32_t Height;
		uint32_t Depth;
		uint32_t CompressScale = 1;
		float	 VoxelSize = 1.0f;
		bool	 Compressed = false;
		bool	 IsOpaque = false;

		std::vector<uint8_t> ColorIndices;
		std::vector<CompressedCell> CompressedCells;


		bool DecompressCell(uint32_t cx, uint32_t cy, uint32_t cz);

		int64_t	Compress(uint32_t scale);
		int64_t	Compress(uint32_t scale, bool& cancel);


		static VoxelSubmesh Compress(uint32_t scale, uint32_t width, uint32_t height, uint32_t depth, float voxelSize, const std::vector<uint8_t>& colorIndices);
	
	private:
		std::vector<CompressedCell> m_FreeSpace;
	};


	struct VoxelTransformAnimation
	{
		std::vector<glm::mat4> Transforms;
		std::vector<uint32_t> Keyframes;
		bool Loop;
	};

	struct VoxelModelAnimation
	{
		std::vector<uint32_t> SubmeshIndices;
		bool Loop;
	};

	struct VoxelInstance
	{
		glm::mat4 Transform;
		uint32_t  SubmeshIndex;

		VoxelTransformAnimation TransformAnimation;
		VoxelModelAnimation     ModelAnimation;
	};

	struct VoxelColor
	{
		uint8_t R, G, B, A;
	};

	class XYZ_API VoxelMeshSource : public Asset
	{
	public:
		VoxelMeshSource(const std::string& filepath);

		virtual AssetType GetAssetType() const override { return AssetType::VoxelMeshSource; }
		static AssetType	GetStaticType() { return AssetType::VoxelMeshSource; }

		const AABB&							GetAABB()		  const { return m_AABB; }
		const std::array<VoxelColor, 256>&	GetColorPallete() const { return m_ColorPallete; }
		const std::vector<VoxelSubmesh>&	GetSubmeshes()	  const { return m_Submeshes; }
		const std::vector<VoxelInstance>&	GetInstances()	  const { return m_Instances; }
		uint32_t							GetNumVoxels() const { return m_NumVoxels; }
	private:
		std::string m_Filepath;
		std::array<VoxelColor, 256> m_ColorPallete;
		std::vector<VoxelSubmesh>  m_Submeshes;
		std::vector<VoxelInstance> m_Instances;
		AABB					   m_AABB;
		uint32_t				   m_NumVoxels;
	};
}