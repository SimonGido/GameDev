#include "stdafx.h"
#include "VoxelMesh.h"

#include "XYZ/Utils/Math/AABB.h"


namespace XYZ {
	static uint32_t Index3D(uint32_t x, uint32_t y, uint32_t z, uint32_t width, uint32_t height)
	{
		return x + width * (y + height * z);
	}



	VoxelSourceMesh::VoxelSourceMesh(const Ref<VoxelMeshSource>& meshSource)
		:
		m_MeshSource(meshSource)
	{
	}
	VoxelSourceMesh::VoxelSourceMesh(const AssetHandle& meshSourceHandle)
		:
		m_MeshSource(meshSourceHandle)
	{
	}
	const std::vector<VoxelSubmesh>& VoxelSourceMesh::GetSubmeshes() const
	{
		return m_MeshSource->GetSubmeshes();
	}
	const std::vector<VoxelInstance>& VoxelSourceMesh::GetInstances() const
	{
		return m_MeshSource->GetInstances();
	}
	const AssetHandle& VoxelSourceMesh::GetRenderID() const
	{
		return GetHandle();
	}

	VoxelProceduralMesh::VoxelProceduralMesh()
		:
		m_NumVoxels(0),
		m_NumCompressedCells(0),
		m_ColorPalleteDirty(false)
	{
		memset(&m_ColorPallete, 0, sizeof(VoxelColor) * m_ColorPallete.size());
	}

	void VoxelProceduralMesh::SetSubmeshes(std::vector<VoxelSubmesh>&& submeshes)
	{
		m_DirtySubmeshes.clear();
		m_DirtyCompressedCells.clear();

		m_Submeshes = std::forward<std::vector<VoxelSubmesh>>(submeshes);
		m_NumVoxels = 0;
		m_NumCompressedCells = 0;
		uint32_t index = 0;
		for (auto& submesh : m_Submeshes)
		{
			m_NumVoxels += static_cast<uint32_t>(submesh.ColorIndices.size());
			m_NumCompressedCells += static_cast<uint32_t>(submesh.CompressedCells.size());
			m_DirtySubmeshes[index] = { 0, static_cast<uint32_t>(submesh.ColorIndices.size()) };
			index++;
		}
	}

	void VoxelProceduralMesh::SetSubmeshes(const std::vector<VoxelSubmesh>& submeshes)
	{
		m_DirtySubmeshes.clear();
		m_DirtyCompressedCells.clear();

		m_Submeshes = submeshes;
		m_NumVoxels = 0;
		m_NumCompressedCells = 0;
		uint32_t index = 0;
		for (auto& submesh : m_Submeshes)
		{
			m_NumVoxels += static_cast<uint32_t>(submesh.ColorIndices.size());
			m_NumCompressedCells += static_cast<uint32_t>(submesh.CompressedCells.size());
			m_DirtySubmeshes[index] = { 0, static_cast<uint32_t>(submesh.ColorIndices.size()) };
			index++;
		}		
	}

	void VoxelProceduralMesh::SetInstances(const std::vector<VoxelInstance>& instances)
	{
		m_Instances = instances;
	}

	void VoxelProceduralMesh::SetColorPallete(const std::array<VoxelColor, 256>& pallete)
	{
		m_ColorPallete = pallete;
		m_ColorPalleteDirty = true;
	}

	void VoxelProceduralMesh::SetVoxelColor(uint32_t submeshIndex, uint32_t x, uint32_t y, uint32_t z, uint8_t value)
	{
		auto& submesh = m_Submeshes[submeshIndex];
		const uint32_t index = Index3D(x, y, z, submesh.Width, submesh.Height);
		const uint8_t colorIndex = submesh.ColorIndices[index];
		
		// TODO: check transparency ( Implement proper counter for transparent voxels )

		submesh.ColorIndices[index] = value;
		auto& range = m_DirtySubmeshes[submeshIndex];
		range.Start = std::min(range.Start, index);
		range.End = std::max(range.End, index + 1);
	}

	void VoxelProceduralMesh::DecompressCell(uint32_t submeshIndex, uint32_t cx, uint32_t cy, uint32_t cz)
	{
		auto& submesh = m_Submeshes[submeshIndex];
		const uint32_t cIndex = Index3D(cx, cy, cz, submesh.Width, submesh.Height);

		const uint32_t origCount = static_cast<uint32_t>(submesh.ColorIndices.size());
		
		if (submesh.DecompressCell(cx, cy, cz))
		{
			m_NumVoxels -= origCount;
			m_NumVoxels += static_cast<uint32_t>(submesh.ColorIndices.size());

			auto start = submesh.CompressedCells[cIndex].VoxelOffset;
			auto end = start + static_cast<uint32_t>(std::pow(submesh.CompressScale, 3u));

			auto& range = m_DirtySubmeshes[submeshIndex];
			range.Start = std::min(range.Start, start);
			range.End = std::max(range.End, end);

			m_DirtyCompressedCells[submeshIndex].push_back(cIndex);
		}	
	}

	const std::array<VoxelColor, 256>& VoxelProceduralMesh::GetColorPallete() const
	{
		return m_ColorPallete;
	}

	const std::vector<VoxelSubmesh>& VoxelProceduralMesh::GetSubmeshes() const
	{
		return m_Submeshes;
	}
	const std::vector<VoxelInstance>& VoxelProceduralMesh::GetInstances() const
	{
		return m_Instances;
	}

	const AssetHandle& VoxelProceduralMesh::GetRenderID() const
	{
		return GetHandle();
	}

	
	bool VoxelProceduralMesh::ColorPalleteDirty() const
	{
		bool dirty = m_ColorPalleteDirty;
		m_ColorPalleteDirty = false;
		return dirty;
	}

	std::unordered_map<uint32_t, VoxelMesh::DirtyRange> VoxelProceduralMesh::DirtySubmeshes() const
	{
		return std::move(m_DirtySubmeshes);
	}

	std::unordered_map<uint32_t, std::vector<uint32_t>> VoxelProceduralMesh::DirtyCompressedCells() const
	{
		return std::move(m_DirtyCompressedCells);
	}
	
}