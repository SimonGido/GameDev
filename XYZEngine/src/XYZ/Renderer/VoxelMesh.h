#pragma once

#include "XYZ/Core/Ref/Ref.h"


#include "XYZ/Asset/Renderer/VoxelMeshSource.h"
#include "XYZ/Asset/AssetReference.h"

#include <glm/glm.hpp>


namespace XYZ {


	class XYZ_API VoxelMesh : public Asset
	{
	public:
		virtual ~VoxelMesh() = default;
		virtual const std::array<VoxelColor, 256>& GetColorPallete() const = 0;
		virtual const std::vector<VoxelSubmesh>& GetSubmeshes() const = 0;
		virtual const std::vector<VoxelInstance>& GetInstances() const = 0;
		virtual const AssetHandle& GetRenderID() const = 0;
		virtual uint32_t GetNumVoxels() const = 0;
		virtual uint32_t GetNumCompressedCells() const = 0;

	protected:
		struct DirtyRange
		{
			uint32_t Start = std::numeric_limits<uint32_t>::max();
			uint32_t End = 0;
		};
		
		virtual bool ColorPalleteDirty() const = 0;

		virtual std::unordered_map<uint32_t, DirtyRange>			DirtySubmeshes() const { return {}; }
		virtual std::unordered_map<uint32_t, std::vector<uint32_t>> DirtyCompressedCells() const { return {}; }

		friend class VoxelRenderer;
	};

	class XYZ_API VoxelSourceMesh : public VoxelMesh
	{
	public:
		VoxelSourceMesh(const Ref<VoxelMeshSource>& meshSource);
		VoxelSourceMesh(const AssetHandle& meshSourceHandle);
		
		virtual AssetType GetAssetType() const override { return AssetType::VoxelSourceMesh; }


		AssetReference<VoxelMeshSource> GetMeshSource() const { return m_MeshSource; }
		
		virtual const std::array<VoxelColor, 256>& GetColorPallete() const override { return m_MeshSource->GetColorPallete(); }
		virtual const std::vector<VoxelSubmesh>& GetSubmeshes() const override;
		virtual const std::vector<VoxelInstance>& GetInstances() const override;
		virtual const AssetHandle& GetRenderID() const override;
		virtual uint32_t GetNumVoxels() const override { return m_MeshSource->GetNumVoxels(); }
		virtual uint32_t GetNumCompressedCells() const override { return 0; }

		static AssetType GetStaticType() { return AssetType::VoxelSourceMesh; }
	
	private:
		virtual bool ColorPalleteDirty() const { return false; }

	private:
		AssetReference<VoxelMeshSource> m_MeshSource;
	};

	class XYZ_API VoxelProceduralMesh : public VoxelMesh
	{
	public:
		VoxelProceduralMesh();

		virtual AssetType GetAssetType() const override { return AssetType::None; }

		void SetSubmeshes(std::vector<VoxelSubmesh>&& submeshes);
		void SetSubmeshes(const std::vector<VoxelSubmesh>& submeshes);
		void SetInstances(const std::vector<VoxelInstance>& instances);
		void SetColorPallete(const std::array<VoxelColor, 256>& pallete);
		void SetVoxelColor(uint32_t submeshIndex, uint32_t x, uint32_t y, uint32_t z, uint8_t value);

		void DecompressCell(uint32_t submeshIndex, uint32_t cx, uint32_t cy, uint32_t cz);

		virtual const std::array<VoxelColor, 256>& GetColorPallete() const override;
		virtual const std::vector<VoxelSubmesh>& GetSubmeshes() const override;
		virtual const std::vector<VoxelInstance>& GetInstances() const override;
		virtual const AssetHandle& GetRenderID() const override;
		virtual uint32_t GetNumVoxels() const override { return m_NumVoxels; }
		virtual uint32_t GetNumCompressedCells() const override { return m_NumCompressedCells; }

		static AssetType GetStaticType() { return AssetType::None; }

	private:
		virtual bool												ColorPalleteDirty() const override;
		virtual std::unordered_map<uint32_t, DirtyRange>			DirtySubmeshes() const override;
		virtual std::unordered_map<uint32_t, std::vector<uint32_t>> DirtyCompressedCells() const override;

	private:
		std::vector<VoxelSubmesh>	m_Submeshes;
		std::vector<VoxelInstance>	m_Instances;
		std::array<VoxelColor, 256> m_ColorPallete;

		
		mutable std::unordered_map<uint32_t, DirtyRange>			m_DirtySubmeshes;	
		mutable std::unordered_map<uint32_t, std::vector<uint32_t>> m_DirtyCompressedCells;
		mutable bool												m_ColorPalleteDirty;

		uint32_t m_NumVoxels;
		uint32_t m_NumCompressedCells;
	};
}