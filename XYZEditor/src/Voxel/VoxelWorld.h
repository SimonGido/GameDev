#pragma once
#include "XYZ/Utils/Math/Ray.h"
#include "XYZ/Utils/DataStructures/Octree.h"
#include "XYZ/Renderer/VoxelMesh.h"
#include "XYZ/Utils/DataStructures/ThreadQueue.h"

#include <glm/glm.hpp>

namespace XYZ{


	struct VoxelChunk
	{
		~VoxelChunk();

		int64_t X = 0;
		int64_t Z = 0;

		Ref<VoxelProceduralMesh> Mesh;
		std::vector<uint8_t>	 ColorIndices;
		std::vector<glm::vec4>	 GrassPositions;
	};

	struct VoxelBiom
	{
		float	 Frequency;
		uint32_t Octaves;
		std::array<VoxelColor, 256> ColorPallete;
	};

	class VoxelWorld
	{
	public:
		static constexpr glm::ivec3 sc_ChunkDimensions = glm::ivec3(512, 512, 512);
		static constexpr uint32_t	sc_ChunkViewDistance = 3; // View distance from center
		static constexpr int64_t    sc_MaxVisibleChunksPerAxis = sc_ChunkViewDistance * 2 + 1;
		static constexpr float      sc_ChunkVoxelSize = 1.0f;
		static constexpr glm::vec3  sc_GrassSize = { 0.2f, 0.5f, 0.2f };

		static ThreadQueue<std::vector<uint8_t>> DataPool;

		using ActiveChunkStorage = array_grid2D<sc_MaxVisibleChunksPerAxis, VoxelChunk>;
	public:
		VoxelWorld(const std::filesystem::path& worldPath, uint32_t seed);
		
		void Update(const glm::vec3& position);
		void ProcessGenerated();

		const std::unique_ptr<ActiveChunkStorage>& GetActiveChunks() const;
	private:
		void generateChunks(int64_t centerChunkX, int64_t centerChunkZ);

		std::unique_ptr<ActiveChunkStorage> shiftChunks(int64_t dirX, int64_t dirZ);

		VoxelChunk generateChunk(int64_t chunkX, int64_t chunkZ, const VoxelBiom& biom, bool& cancel);

		void generateGrassPosition(uint32_t voxelX, uint32_t voxelY, uint32_t voxelZ, float voxelSize, std::vector<glm::vec4>& grassPositions);

	private:
		struct GeneratedChunk
		{
			int64_t IndexX;
			int64_t IndexZ;
			
			VoxelChunk Chunk;
			bool Finished = false;
			bool Canceled = false;
		};


	private:
		std::unique_ptr<ActiveChunkStorage> m_ActiveChunks;

		std::filesystem::path m_WorldPath;

		std::unordered_map<std::string, VoxelBiom> m_Bioms;

		std::vector<std::shared_ptr<GeneratedChunk>> m_ChunksGenerated;


		

		uint32_t m_Seed;
		int64_t m_LastCenterChunkX = 0;
		int64_t m_LastCenterChunkZ = 0;
	};

}
