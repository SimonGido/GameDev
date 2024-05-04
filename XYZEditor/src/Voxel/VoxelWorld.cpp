#include "stdafx.h"
#include "VoxelWorld.h"

#include "XYZ/Utils/Math/Perlin.h"
#include "XYZ/Utils/Random.h"

namespace XYZ {

	static uint32_t Index3D(uint32_t x, uint32_t y, uint32_t z, uint32_t width, uint32_t height)
	{
		return x + width * (y + height * z);
	}

	static uint32_t Index3D(const glm::uvec3& index, uint32_t width, uint32_t height)
	{
		return index.x + width * (index.y + height * index.z);
	}

	static uint32_t Index2D(uint32_t x, uint32_t z, uint32_t depth)
	{
		return x * depth + z;
	}


	static void RotateGridXZ(uint8_t* arr, uint32_t size)
	{
		std::vector<uint8_t> result(size * size * size, 0);

		for (uint32_t x = 0; x < size; ++x)
		{
			for (uint32_t y = 0; y < size; ++y)
			{
				for (uint32_t z = 0; z < size; ++z)
				{
					const uint32_t origIndex = Index3D(x, y, z, size, size);
					const uint32_t rotIndex = Index3D(z, y, x, size, size);
					result[rotIndex] = arr[origIndex];
				}
			}
		}
		memcpy(arr, result.data(), result.size());
	}

	static bool IsBlockUniform(const std::vector<uint8_t>& arr, const glm::uvec3& start, const glm::uvec3& end, uint32_t width, uint32_t height)
	{
		const uint32_t oldIndex = Index3D(start.x, start.y, start.z, width, height);
		const uint8_t oldColorIndex = arr[oldIndex];
		for (uint32_t x = start.x; x < end.x; ++x)
		{
			for (uint32_t y = start.y; y < end.y; ++y)
			{
				for (uint32_t z = start.z; z < end.z; ++z)
				{
					const uint32_t newIndex = Index3D(x, y, z, width, height);
					const uint8_t newColorIndex = arr[newIndex];
					if (newColorIndex != oldColorIndex)
						return false;
				}
			}
		}
		return true;
	}

	ThreadQueue<std::vector<uint8_t>> VoxelWorld::DataPool;

	VoxelWorld::VoxelWorld(const std::filesystem::path& worldPath, uint32_t seed)
		:
		m_WorldPath(worldPath),
		m_Seed(seed)
	{
		VoxelBiom& forestBiom = m_Bioms["Forest"];
		forestBiom.ColorPallete[0] = { 0, 0, 0, 0 };
		forestBiom.ColorPallete[1] = { 1, 60, 32, 255 }; // Grass
		forestBiom.ColorPallete[2] = { 1, 30, 230, 50}; // Water
		forestBiom.Octaves = 3;
		forestBiom.Frequency = 1.0f;
		m_ActiveChunks = std::make_unique<ActiveChunkStorage>();

		Perlin::SetSeed(seed);
		generateChunks(0, 0);
	}
	void VoxelWorld::Update(const glm::vec3& position)
	{
		constexpr uint32_t halfDimensionX = sc_ChunkDimensions.x / 2;
		constexpr uint32_t halfDimensionZ = sc_ChunkDimensions.z / 2;


		ProcessGenerated();


		const int64_t centerChunkX = static_cast<int64_t>(std::floor((position.x + halfDimensionX) / sc_ChunkDimensions.x));
		const int64_t centerChunkZ = static_cast<int64_t>(std::floor((position.z + halfDimensionZ) / sc_ChunkDimensions.z));
		
		const int64_t shiftDirX = m_LastCenterChunkX - centerChunkX;
		const int64_t shiftDirZ = m_LastCenterChunkZ - centerChunkZ;
		if (shiftDirX == 0 && shiftDirZ == 0)
			return;

		
		if (shiftDirX != 0 || shiftDirZ != 0)
			m_ActiveChunks = std::move(shiftChunks(shiftDirX, shiftDirZ));

		for (auto gen : m_ChunksGenerated)
			gen->Canceled = true;
		m_ChunksGenerated.clear();

		generateChunks(centerChunkX, centerChunkZ);
		m_LastCenterChunkX = centerChunkX;
		m_LastCenterChunkZ = centerChunkZ;
	}
	void VoxelWorld::ProcessGenerated()
	{
		for (auto it = m_ChunksGenerated.begin(); it != m_ChunksGenerated.end(); )
		{
			std::shared_ptr<GeneratedChunk> chunk = (*it);
			if (chunk->Finished)
			{
				(*m_ActiveChunks)[chunk->IndexX][chunk->IndexZ] = std::move(chunk->Chunk);
				it = m_ChunksGenerated.erase(it);
			}
			else
			{
				it++;
			}
		}
	}
	const VoxelChunk* VoxelWorld::GetVoxelChunk(const glm::vec3& position) const
	{
		constexpr uint32_t halfDimensionX = sc_ChunkDimensions.x / 2;
		constexpr uint32_t halfDimensionZ = sc_ChunkDimensions.z / 2;

		const int64_t chunkX = static_cast<int64_t>(std::floor((position.x + halfDimensionX) / sc_ChunkDimensions.x)) + sc_ChunkViewDistance;
		const int64_t chunkZ = static_cast<int64_t>(std::floor((position.z + halfDimensionZ) / sc_ChunkDimensions.z)) + sc_ChunkViewDistance;

		const int64_t clampedX = abs(chunkX) % sc_MaxVisibleChunksPerAxis;
		const int64_t clampedZ = abs(chunkZ) % sc_MaxVisibleChunksPerAxis;

		return &(*m_ActiveChunks)[clampedX][clampedZ];
	}
	const std::unique_ptr<VoxelWorld::ActiveChunkStorage>& VoxelWorld::GetActiveChunks() const
	{
		return m_ActiveChunks;
	}
	void VoxelWorld::generateChunks(int64_t centerChunkX, int64_t centerChunkZ)
	{
		const int64_t chunksWidth = sc_MaxVisibleChunksPerAxis;

		auto& pool = Application::Get().GetThreadPool();
		VoxelBiom& forestBiom = m_Bioms["Forest"];
		for (int64_t chunkX = 0; chunkX < chunksWidth; chunkX++)
		{
			for (int64_t chunkZ = 0; chunkZ < chunksWidth; chunkZ++)
			{
				const int64_t worldChunkX = chunkX + centerChunkX - sc_ChunkViewDistance;
				const int64_t worldChunkZ = chunkZ + centerChunkZ - sc_ChunkViewDistance;

				if (!(*m_ActiveChunks)[chunkX][chunkZ].Mesh.Raw()) // Chunk was shifted away
				{
					m_ChunksGenerated.push_back(std::make_shared<GeneratedChunk>());
					std::shared_ptr<GeneratedChunk> gen = m_ChunksGenerated.back();
					gen->IndexX = chunkX;
					gen->IndexZ = chunkZ;
					pool.PushJob([this, gen, worldChunkX, worldChunkZ, &forestBiom]() {
						gen->Chunk = generateChunk(worldChunkX, worldChunkZ, forestBiom, gen->Canceled);
						gen->Finished = true;
					});
				}
			}
		}
	}
	std::unique_ptr<VoxelWorld::ActiveChunkStorage> VoxelWorld::shiftChunks(int64_t dirX, int64_t dirZ)
	{
		std::unique_ptr<ActiveChunkStorage> shiftedChunks = std::make_unique<ActiveChunkStorage>();
		if (dirX > sc_ChunkViewDistance || dirZ > sc_ChunkViewDistance)
			return shiftedChunks;

		const int64_t chunksWidth = sc_MaxVisibleChunksPerAxis;
		for (int64_t chunkX = 0; chunkX < chunksWidth; chunkX++)
		{
			const int64_t shiftedChunkX = chunkX + dirX;
			if (shiftedChunkX >= sc_MaxVisibleChunksPerAxis || shiftedChunkX < 0)
				continue;

			for (int64_t chunkZ = 0; chunkZ < chunksWidth; chunkZ++)
			{
				const int64_t shiftedChunkZ = chunkZ + dirZ;
				if (shiftedChunkZ >= sc_MaxVisibleChunksPerAxis || shiftedChunkZ < 0)
					continue;

				(*shiftedChunks)[shiftedChunkX][shiftedChunkZ] = std::move((*m_ActiveChunks)[chunkX][chunkZ]);
			}
		}
		return shiftedChunks;
	}
	VoxelChunk VoxelWorld::generateChunk(int64_t chunkX, int64_t chunkZ, const VoxelBiom& biom, bool& cancel)
	{
		VoxelChunk chunk;		
		chunk.X = chunkX;
		chunk.Z = chunkZ;

		chunk.Mesh = Ref<VoxelProceduralMesh>::Create();
		chunk.Mesh->SetColorPallete(biom.ColorPallete);

		VoxelSubmesh submesh;
		submesh.Width = sc_ChunkDimensions.x;
		submesh.Height = sc_ChunkDimensions.y;
		submesh.Depth = sc_ChunkDimensions.z;
		submesh.VoxelSize = sc_ChunkVoxelSize;
		submesh.IsOpaque = false;

		VoxelSubmesh waterSubmesh;
		waterSubmesh.Width = sc_ChunkDimensions.x;
		waterSubmesh.Height = sc_ChunkDimensions.y;
		waterSubmesh.Depth = sc_ChunkDimensions.z;
		waterSubmesh.VoxelSize = sc_ChunkVoxelSize;
		waterSubmesh.IsOpaque = false;
	
		const glm::vec3 centerTranslation = -glm::vec3(
			submesh.Width / 2.0f * submesh.VoxelSize,
			submesh.Height / 2.0f * submesh.VoxelSize,
			submesh.Depth / 2.0f * submesh.VoxelSize
		);

		const glm::vec3 translation = glm::vec3(
			chunk.X * sc_ChunkDimensions.x * sc_ChunkVoxelSize, 
			0.0f, 
			chunk.Z * sc_ChunkDimensions.z * sc_ChunkVoxelSize
		);

		VoxelInstance instance;
		instance.SubmeshIndex = 0;
		instance.Transform = glm::translate(glm::mat4(1.0f), translation + centerTranslation);

		VoxelInstance waterInstance;
		waterInstance.SubmeshIndex = 1;
		waterInstance.Transform = glm::translate(glm::mat4(1.0f), translation + centerTranslation);



		const double fx = static_cast<double>(biom.Frequency / submesh.Width);
		const double fz = static_cast<double>(biom.Frequency / submesh.Depth);

		
		if (!DataPool.Empty())
			submesh.ColorIndices = DataPool.PopBack();

		waterSubmesh.ColorIndices.resize(submesh.Width * submesh.Height * submesh.Depth, 0);
		submesh.ColorIndices.resize(submesh.Width * submesh.Height * submesh.Depth, 0);
		
		for (uint32_t x = 0; x < submesh.Width; ++x)
		{
			for (uint32_t z = 0; z < submesh.Depth; ++z)
			{
				const double xDouble = static_cast<double>(x) + chunk.X * sc_ChunkDimensions.x;
				const double zDouble = static_cast<double>(z) + chunk.Z * sc_ChunkDimensions.z;
				const double val = Perlin::Octave2D(xDouble * fx, zDouble * fz, biom.Octaves);
				const uint32_t genHeight = val * submesh.Height;

				for (uint32_t y = 0; y < genHeight; y++)
				{
					if (cancel)
						return chunk;

					const uint32_t index = Index3D(x, y, z, submesh.Width, submesh.Height);
					submesh.ColorIndices[index] = 1; // Grass
				}


				const uint32_t waterHeight = submesh.Height / 2;
				for (uint32_t y = genHeight; y < waterHeight; y++)
				{
					if (cancel)
						return chunk;

					const uint32_t index = Index3D(x, y, z, submesh.Width, submesh.Height);
					submesh.ColorIndices[index] = 2; // Water
				}
				if (genHeight >= waterHeight)
				{			
					//generateGrassPosition(x, genHeight, z, submesh.VoxelSize, chunk.GrassPositions);
				}
			}
		}

		submesh.Compress(16, cancel);
		chunk.Mesh->SetSubmeshes({ submesh });
		chunk.Mesh->SetInstances({ instance });

		return chunk;
	}
	void VoxelWorld::generateGrassPosition(uint32_t voxelX, uint32_t voxelY, uint32_t voxelZ, float voxelSize, std::vector<glm::vec4>& grassPositions)
	{
		const glm::vec2 grassScaleRange = { 0.6f, 1.0f };
		// NOTE: this is assuming that sc_GrassSize.x and sc_GrassSize.z are equal
		uint32_t grassBladesPerVoxelRow = static_cast<uint32_t>(floor(voxelSize / sc_GrassSize.x));
		
		uint32_t grassCount = 0;
		for (uint32_t x = 0; x < grassBladesPerVoxelRow; x++)
		{
			for (uint32_t z = 0; z < grassBladesPerVoxelRow; z++)
			{
				float grassScale = RandomNumber(grassScaleRange.x, grassScaleRange.y);
				glm::vec4 grassPosition = {
					static_cast<float>(voxelX) * voxelSize + sc_GrassSize.x / 2.0f,
					static_cast<float>(voxelY) * voxelSize + sc_GrassSize.y * grassScale / 2.0f,
					static_cast<float>(voxelZ) * voxelSize + sc_GrassSize.z / 2.0f,
					grassScale
				};
				if (RandomBool(0.2f))
				{
					glm::vec3 grassOffset = {
						x * sc_GrassSize.x,
						0.0f,
						z * sc_GrassSize.z
					};
					grassPositions.push_back(grassPosition + glm::vec4(grassOffset, 0.0f));
					grassCount++;
				}
			}
		}
	}




	VoxelChunk::~VoxelChunk()
	{
		if (!ColorIndices.empty())
			VoxelWorld::DataPool.EmplaceBack(std::move(ColorIndices));
	}
}
