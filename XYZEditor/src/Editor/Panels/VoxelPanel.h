#pragma once

#include "XYZ/Scene/Scene.h"
#include "XYZ/Scene/SceneEntity.h"
#include "XYZ/Scene/SceneIntersection.h"
#include "XYZ/Scene/Components.h"

#include "XYZ/Event/ApplicationEvent.h"
#include "XYZ/Event/InputEvent.h"

#include "XYZ/Renderer/Texture.h"
#include "XYZ/Renderer/VoxelRenderer.h"
#include "XYZ/Utils/Math/Ray.h"
#include "XYZ/Utils/DataStructures/Octree.h"

#include "XYZ/Asset/Renderer/VoxelMeshSource.h"

#include "Editor/EditorPanel.h"
#include "Editor/EditorCamera.h"
#include "Editor/FreeFlyCamera.h"

#include "XYZ/Utils/Algorithms/SpaceColonization.h"

#include "Voxel/VoxelWorld.h"

namespace XYZ {
	namespace Editor {


		struct VoxelTerrain
		{
			VoxelSubmesh Terrain;
			std::vector<uint8_t> WaterMap;
			std::vector<uint16_t> TerrainHeightmap;
		};

		struct SpaceColonizationData
		{
			uint32_t AttractorsCount = 300;
			glm::vec3 AttractorsOffset = glm::vec3(0.0f, 100.0f, 0.0f);
			float AttractorsRadius = 50.0f;
			float AttractionRange = 20.0f;
			float KillRange = 10.0f;
			float BranchLength = 7.0f;
			int32_t TreeRadius = 7;
		};

		class VoxelPanel : public EditorPanel
		{
		public:
			VoxelPanel(std::string name);
			~VoxelPanel();

			virtual void OnImGuiRender(bool& open) override;
			virtual void OnUpdate(Timestep ts) override;
			virtual bool OnEvent(Event& event) override;

			virtual void SetSceneContext(const Ref<Scene>& context);

			void SetVoxelRenderer(const Ref<VoxelRenderer>& voxelRenderer);

			EditorCamera& GetEditorCamera() { return m_EditorCamera; }

		private:
			void handlePanelResize(const glm::vec2& newSize);
			void drawTransform(TransformComponent& transform, int id) const;
			void drawSpaceColonizationData();

			void pushGenerateVoxelMeshJob();
			void submitWater();
			
			static VoxelTerrain generateVoxelTerrainMesh(
				uint32_t seed, uint32_t frequency, uint32_t octaves,
				uint32_t width, uint32_t height, uint32_t depth
			);

		private:
			Ref<VoxelRenderer>			m_VoxelRenderer;
			glm::vec2				    m_ViewportSize;
			std::array<glm::vec2, 2>	m_ViewportBounds{};

			bool m_ViewportFocused;
			bool m_ViewportHovered;

			FreeFlyCamera m_FreeFlyCamera;
			EditorCamera m_EditorCamera;
			
			std::vector<TransformComponent> m_TreeTransforms;
			std::vector<TransformComponent> m_Transforms;


			Ref<VoxelSourceMesh> m_CastleMesh;
			Ref<VoxelSourceMesh> m_KnightMesh;
			Ref<VoxelSourceMesh> m_DeerMesh;
			Ref<VoxelProceduralMesh> m_ProceduralMesh;

			Ref<VoxelProceduralMesh> m_TreeMesh;

			Ref<MaterialAsset> m_WaterMaterial;
			
			static constexpr uint8_t Empty = 0;
			static constexpr uint8_t Water = 1;
			static constexpr uint8_t Snow = 2;
			static constexpr uint8_t Grass = 3;
			static constexpr uint8_t Wood = 4;
			static constexpr uint8_t Leaves = 5;

			uint32_t m_DeerKeyFrame = 0;
			float	 m_KeyLength = 0.3f;
			float	 m_CurrentTime = 0.0f;

			uint32_t m_Frequency = 2;
			uint32_t m_Octaves = 3;
			uint32_t m_Seed = 60;

			VoxelTerrain m_Terrain;
			std::future<VoxelTerrain> m_GenerateVoxelsFuture;
	
			bool m_Generating = false;

			StorageBufferAllocation m_WaterDensityAllocation;
			bool m_UpdateWater = false;
		

			SpaceColonization* m_SpaceColonization = nullptr;
			SpaceColonizationData m_SpaceColonizationData;


			Octree m_Octree;
			VoxelWorld m_World;
		};
	}
}