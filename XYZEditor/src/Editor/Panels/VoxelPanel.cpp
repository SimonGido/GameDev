#include "stdafx.h"
#include "VoxelPanel.h"

#include "XYZ/Core/Input.h"
#include "XYZ/Core/Application.h"

#include "XYZ/Renderer/SceneRenderer.h"
#include "XYZ/Renderer/Renderer2D.h"
#include "XYZ/Renderer/Renderer2D.h"


#include "XYZ/Utils/Math/AABB.h"
#include "XYZ/Utils/Math/Math.h"
#include "XYZ/Utils/Math/Perlin.h"
#include "XYZ/Utils/Random.h"
#include "XYZ/Utils/Algorithms/Raymarch.h"


#include "XYZ/ImGui/ImGui.h"

#include "Editor/Event/EditorEvents.h"
#include "Editor/EditorHelper.h"
#include "EditorLayer.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/random.hpp>


namespace XYZ {
	namespace Editor {

		
		namespace Utils {
			
			static uint8_t RandomColorIndex(uint32_t x = 0, uint32_t y = 255)
			{
				return static_cast<uint8_t>(RandomNumber(x, y));
			}

			static uint32_t Index3D(int x, int y, int z, int width, int height)
			{
				return x + width * (y + height * z);
			}

			static uint32_t Index3D(const glm::ivec3& index, int width, int height)
			{
				return index.x + width * (index.y + height * index.z);
			}


			static uint32_t Index2D(int x, int z, int depth)
			{
				return x * depth + z;
			}

			static AABB VoxelModelToAABB(const glm::mat4& transform, uint32_t width, uint32_t height, uint32_t depth, float voxelSize)
			{
				AABB result;
				glm::vec3 min = glm::vec3(0.0f);
				glm::vec3 max = glm::vec3(width, height, depth) * voxelSize;

				result = result.TransformAABB(transform);
				return result;
			}

			static std::array<glm::vec2, 2> ImGuiViewportBounds()
			{
				const auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
				const auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
				const auto viewportOffset = ImGui::GetWindowPos();

				std::array<glm::vec2, 2> result;
				result[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
				result[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };
				return result;
			}
		}

		static constexpr float sc_TerrainVoxelSize = 1.0f;

		VoxelPanel::VoxelPanel(std::string name)
			:
			EditorPanel(std::forward<std::string>(name)),
			m_ViewportSize(0.0f),
			m_EditorCamera(30.0f, 1.778f, 0.1f, 10000.0f),
			m_Octree(AABB(glm::vec3(0.0f), glm::vec3(0.0f)), 10),
			m_World("blabla", 50)
		{
			m_DeerMesh = Ref<VoxelSourceMesh>::Create(Ref<VoxelMeshSource>::Create("Assets/Voxel/anim/deer.vox"));
			m_CastleMesh = Ref<VoxelSourceMesh>::Create(Ref<VoxelMeshSource>::Create("Assets/Voxel/castle.vox"));
			m_KnightMesh = Ref<VoxelSourceMesh>::Create(Ref<VoxelMeshSource>::Create("Assets/Voxel/chr_knight.vox"));

			m_ProceduralMesh = Ref<VoxelProceduralMesh>::Create();
			m_TreeMesh = Ref<VoxelProceduralMesh>::Create();

			VoxelSubmesh submesh;
			submesh.Width = 512;
			submesh.Height = 512;
			submesh.Depth = 512;
			submesh.VoxelSize = sc_TerrainVoxelSize;
	
			submesh.ColorIndices.resize(submesh.Width * submesh.Height * submesh.Depth, 0);
		
			VoxelInstance instance;
			instance.SubmeshIndex = 0;
			instance.Transform = glm::translate(glm::mat4(1.0f), -glm::vec3(
				submesh.Width / 2.0f * submesh.VoxelSize,
				submesh.Height / 2.0f * submesh.VoxelSize,
				submesh.Depth / 2.0f * submesh.VoxelSize
			));
	
			auto colorPallete = m_KnightMesh->GetColorPallete();
			
			colorPallete[Water] = { 0, 100, 220, 50 };
			colorPallete[Snow] = { 255, 255, 255, 255 }; // Snow
			colorPallete[Grass] = { 1, 60, 32, 255 }; // Grass
			colorPallete[Wood] = { 150, 75, 0, 255 };
			colorPallete[Leaves] = { 1, 100, 40, 255 };

			m_ProceduralMesh->SetColorPallete(colorPallete);
			m_ProceduralMesh->SetSubmeshes({ submesh});
			m_ProceduralMesh->SetInstances({ instance });

			VoxelSubmesh treeSubmesh;
			treeSubmesh.Width = 100;
			treeSubmesh.Height = 300;
			treeSubmesh.Depth = 100;
			treeSubmesh.VoxelSize = 1.0f;

			treeSubmesh.ColorIndices.resize(treeSubmesh.Width * treeSubmesh.Height * treeSubmesh.Depth, 0);

			m_TreeMesh->SetColorPallete(colorPallete);
			m_TreeMesh->SetSubmeshes({ treeSubmesh });
			m_TreeMesh->SetInstances({ instance });

			m_TreeTransforms.push_back({});

			uint32_t count = 30;
			m_Transforms.resize(count);
	
			const glm::vec3 halfTerrainSize(
				submesh.Width / 2.0 * submesh.VoxelSize,
				submesh.Height / 2.0 * submesh.VoxelSize,
				submesh.Depth / 2.0 * submesh.VoxelSize
			);

			float xOffset = -200;
			float zOffset = -160;
			for (uint32_t i = 0; i < count; ++i)
			{
				m_Transforms[i].GetTransform().Translation.x = RandomNumber(-1200.0f, 1200.0f);
				m_Transforms[i].GetTransform().Translation.y = RandomNumber(5.0f, 30.0f);
				m_Transforms[i].GetTransform().Translation.z = RandomNumber(-1200.0f, 1200.0f);
				m_Transforms[i].GetTransform().Rotation.x = glm::radians(-90.0f);
				xOffset += 30.0f;
			}
			pushGenerateVoxelMeshJob();

			Ref<Shader> waterShader = Shader::Create("Resources/Shaders/Voxel/WaterTest.glsl");
			Ref<ShaderAsset> waterShaderAsset = Ref<ShaderAsset>::Create(waterShader);
			m_WaterMaterial = Ref<MaterialAsset>::Create(waterShaderAsset);
		}

		VoxelPanel::~VoxelPanel()
		{
		}

		void VoxelPanel::OnImGuiRender(bool& open)
		{
			{
				UI::ScopedStyleStack styleStack(true, ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
				if (ImGui::Begin("Scene", &open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
				{
					if (m_VoxelRenderer.Raw())
					{
						const ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
						m_ViewportBounds = Utils::ImGuiViewportBounds();
						m_ViewportFocused = ImGui::IsWindowFocused();
						m_ViewportHovered = ImGui::IsWindowHovered();

						ImGuiLayer* imguiLayer = Application::Get().GetImGuiLayer();
						const bool blocked = imguiLayer->GetBlockedEvents();
						// Only unlock possible here
						imguiLayer->BlockEvents(blocked && !m_ViewportFocused && !m_ViewportHovered);


						UI::Image(m_VoxelRenderer->GetFinalPassImage(), viewportPanelSize);
			
						handlePanelResize({ viewportPanelSize.x, viewportPanelSize.y });
					}
				}
				if (m_VoxelRenderer.Raw())
				{
					m_VoxelRenderer->OnImGuiRender();
				}
				ImGui::End();
			}
			if (ImGui::Begin("Voxels Generator"))
			{
				drawSpaceColonizationData();
				ImGui::NewLine();

				ImGui::DragInt("Frequency", (int*)&m_Frequency, 1.0f, 0, INT_MAX);
				ImGui::DragInt("Octaves", (int*)&m_Octaves, 1.0f, 0, INT_MAX);
				ImGui::DragInt("Seed", (int*)&m_Seed, 1.0f, 0, INT_MAX);

				if (ImGui::Button("Apply") && !m_Generating)
				{
					pushGenerateVoxelMeshJob();
				}

				ImGui::Checkbox("Update Water", &m_UpdateWater);
			}
			ImGui::End();

			if (ImGui::Begin("Transforms"))
			{
				int id = 0;

				//for (auto& transform : m_TreeTransforms)
				//{
				//	ImGui::Text("%d", id);
				//	drawTransform(transform, id++);
				//}
				for (auto& transform : m_Transforms)
				{
					ImGui::Text("%d", id);
					drawTransform(transform, id++);			
				}
				ImGui::NewLine();
			}
			ImGui::End();
		}

		void VoxelPanel::OnUpdate(Timestep ts)
		{
			if (m_VoxelRenderer.Raw())
			{			
				m_EditorCamera.OnUpdate(ts);
				
				const glm::mat4 mvp = m_EditorCamera.GetViewProjection();
				
				if (!m_Generating && m_GenerateVoxelsFuture.valid())
				{
					m_Terrain = std::move(m_GenerateVoxelsFuture.get());
					m_ProceduralMesh->SetSubmeshes({ m_Terrain.Terrain });
					
					//m_VoxelRenderer->SubmitComputeData(m_Terrain.WaterMap.data(), m_Terrain.WaterMap.size(), 0, m_WaterDensityAllocation, true);
				}

				m_VoxelRenderer->BeginScene({
					m_EditorCamera.GetViewProjection(),
					m_EditorCamera.GetViewMatrix(),
					m_EditorCamera.GetProjectionMatrix(),
					m_EditorCamera.GetPosition(),
					m_EditorCamera.CreateFrustum()
				});
				
				if (Input::IsKeyPressed(KeyCode::KEY_SPACE))
				{
					m_ProceduralMesh->DecompressCell(0, 0, 0, 0);
					//auto& submesh = m_ProceduralMesh->GetSubmeshes()[0];
					//for (uint32_t y = 0; y < 400; ++y)
					//{
					//	m_ProceduralMesh->SetVoxelColor(0, 256, y, 256, RandomNumber(5u, 255u));
					//}
				}
				//m_World.Update(m_EditorCamera.GetPosition());
				m_World.Update(glm::vec3(0));
				int counter = 0;
				std::vector<Ref<VoxelMesh>> newMeshes;
				std::vector<Ref<VoxelMesh>> oldMeshes;
				for (const auto& chunkRow : *m_World.GetActiveChunks())
				{
					for (const auto& chunk : chunkRow)
					{
						if (chunk.Mesh.Raw())
						{
							bool compressed = true;
							//for (auto& submesh : chunk.Mesh->GetSubmeshes())
							//	compressed &= submesh.Compressed;
							//if (compressed)
							{
								m_VoxelRenderer->SubmitMesh(chunk.Mesh, glm::mat4(1.0f));
								//if (!m_VoxelRenderer->IsMeshAllocated(chunk.Mesh))
								//	newMeshes.push_back(chunk.Mesh);
								//else
								//	oldMeshes.push_back(chunk.Mesh);
							
							}

						}
						counter++;
						//if (counter == 2)
						//	break;
					}
					//if (counter == 2)
					//	break;

				}


				for (const auto& mesh : oldMeshes)
					m_VoxelRenderer->SubmitMesh(mesh, glm::mat4(1.0f));
				
				for (const auto& mesh : newMeshes)
				{
					if (m_VoxelRenderer->SubmitMesh(mesh, glm::mat4(1.0f)))
						break;
				}
				for (auto& transform : m_TreeTransforms)
				{
					//m_VoxelRenderer->SubmitMesh(m_TreeMesh, transform.GetLocalTransform());
				}
				for (size_t i = 0; i < m_Transforms.size(); i += 3)
				{
					const glm::mat4 castleTransform = m_Transforms[i].GetLocalTransform();
					const glm::mat4 knightTransform = m_Transforms[i + 1].GetLocalTransform();
					const glm::mat4 deerTransform = m_Transforms[i + 2].GetLocalTransform();
				
					m_VoxelRenderer->SubmitMesh(m_CastleMesh, castleTransform);
					break;
					//m_VoxelRenderer->SubmitMesh(m_KnightMesh, knightTransform);
					//m_VoxelRenderer->SubmitMesh(m_DeerMesh, deerTransform, &m_DeerKeyFrame);
				}
				
				submitWater();
				
				if (m_CurrentTime > m_KeyLength)
				{
					const uint32_t numKeyframes = m_DeerMesh->GetMeshSource()->GetInstances()[0].ModelAnimation.SubmeshIndices.size();
					m_DeerKeyFrame = (m_DeerKeyFrame + 1) % numKeyframes;
					m_CurrentTime = 0.0f;
				}
				m_CurrentTime += ts;
				m_VoxelRenderer->EndScene();
			}
		}

		static int32_t radius = 7;
		bool VoxelPanel::OnEvent(Event& event)
		{
			if (m_ViewportHovered && m_ViewportFocused)
				m_EditorCamera.OnEvent(event);

			if (event.GetEventType() == EventType::KeyPressed)
			{
				KeyPressedEvent& keyPressedE = (KeyPressedEvent&)event;
				if (keyPressedE.IsKeyPressed(KeyCode::KEY_M))
				{
					radius = m_SpaceColonizationData.TreeRadius;
					auto treeSubmesh = m_TreeMesh->GetSubmeshes()[0];
					if (m_SpaceColonization != nullptr)
						delete m_SpaceColonization;

					const float voxelSize = treeSubmesh.VoxelSize;
					const glm::vec3 voxelPosition = glm::vec3(treeSubmesh.Width / 2, 0, treeSubmesh.Depth / 2) * voxelSize;

					SCInitializer initializer;
					initializer.AttractorsCount = m_SpaceColonizationData.AttractorsCount;
					initializer.AttractorsCenter = voxelPosition + m_SpaceColonizationData.AttractorsOffset;
					initializer.AttractorsRadius = m_SpaceColonizationData.AttractorsRadius * voxelSize;
					initializer.AttractionRange = m_SpaceColonizationData.AttractionRange * voxelSize;
					initializer.KillRange = m_SpaceColonizationData.KillRange * voxelSize;
					initializer.RootPosition = voxelPosition;
					initializer.BranchLength = m_SpaceColonizationData.BranchLength * voxelSize;


					m_SpaceColonization = new SpaceColonization(initializer);
					memset(treeSubmesh.ColorIndices.data(), 0, treeSubmesh.ColorIndices.size());

					m_SpaceColonization->VoxelizeAttractors(treeSubmesh.ColorIndices, treeSubmesh.Width, treeSubmesh.Height, treeSubmesh.Depth, voxelSize);
					m_TreeMesh->SetSubmeshes({ treeSubmesh });
				}
				else if (keyPressedE.IsKeyPressed(KeyCode::KEY_N))
				{
					if (m_SpaceColonization != nullptr)
					{
						VoxelSubmesh treeSubmesh = m_TreeMesh->GetSubmeshes()[0];
						m_SpaceColonization->Grow(
							treeSubmesh.ColorIndices, 
							treeSubmesh.Width, 
							treeSubmesh.Height, 
							treeSubmesh.Depth, 
							treeSubmesh.VoxelSize,
							radius
						);		
						if (radius > 1 && RandomNumber(0u, 1u) != 0)
							radius--;

						m_TreeMesh->SetSubmeshes({ treeSubmesh });
					}
				}
			}

			return false;
		}

		void VoxelPanel::SetSceneContext(const Ref<Scene>& context)
		{
		}

		void VoxelPanel::SetVoxelRenderer(const Ref<VoxelRenderer>& voxelRenderer)
		{
			m_VoxelRenderer = voxelRenderer;
			//m_VoxelRenderer->CreateComputeAllocation(1024 * 400 * 1024, m_WaterDensityAllocation);
		}


		void VoxelPanel::handlePanelResize(const glm::vec2& newSize)
		{
			if (m_ViewportSize.x != newSize.x || m_ViewportSize.y != newSize.y)
			{
				m_ViewportSize = newSize;
				m_EditorCamera.SetViewportSize(m_ViewportSize.x, m_ViewportSize.y);
				m_VoxelRenderer->SetViewportSize(static_cast<uint32_t>(m_ViewportSize.x), static_cast<uint32_t>(m_ViewportSize.y));
			}
		}

		void VoxelPanel::drawTransform(TransformComponent& transform, int id) const
		{
			ImGui::PushID(id);
			glm::vec3 rotation = glm::degrees(transform->Rotation);
			ImGui::DragFloat3("Translation", glm::value_ptr(transform.GetTransform().Translation), 0.1f);
			if (ImGui::DragFloat3("Rotation", glm::value_ptr(rotation), 0.1f))
			{
				transform.GetTransform().Rotation = glm::radians(rotation);
			}
			ImGui::PopID();
		}
		void VoxelPanel::drawSpaceColonizationData()
		{
			ImGui::Text("Space Colonization Data");
			ImGui::DragInt("Attractors Count", (int*)&m_SpaceColonizationData.AttractorsCount);
			ImGui::DragFloat3("Attractors Offset", (float*)&m_SpaceColonizationData.AttractorsOffset);
			ImGui::DragFloat("Attractors Radius", &m_SpaceColonizationData.AttractorsRadius);
			ImGui::DragFloat("Attractors Range", &m_SpaceColonizationData.AttractionRange);
			ImGui::DragFloat("Kill Range", &m_SpaceColonizationData.KillRange);
			ImGui::DragFloat("Branch Length", &m_SpaceColonizationData.BranchLength);

			ImGui::DragInt("Tree Radius", (int*)&m_SpaceColonizationData.TreeRadius);

		}

		void VoxelPanel::pushGenerateVoxelMeshJob()
		{
			m_Generating = true;

			uint32_t seed = m_Seed;
			uint32_t frequency = m_Frequency;
			uint32_t octaves = m_Octaves;

			auto& terrainSubmesh = m_ProceduralMesh->GetSubmeshes()[0];

			uint32_t width = terrainSubmesh.Width;
			uint32_t height = terrainSubmesh.Height;
			uint32_t depth = terrainSubmesh.Depth;
			if (!terrainSubmesh.CompressedCells.empty())
			{
				width *= terrainSubmesh.CompressScale;
				height *= terrainSubmesh.CompressScale;
				depth *= terrainSubmesh.CompressScale;
			}
	
			auto& pool = Application::Get().GetThreadPool();
			m_GenerateVoxelsFuture = pool.SubmitJob([this, seed, frequency, octaves, width, height, depth]() {

				auto terrain = generateVoxelTerrainMesh(
					seed, frequency, octaves,
					width, height, depth
				);
				
				m_Generating = false;
				return terrain;
			});
		}
		void VoxelPanel::submitWater()
		{
			if (m_UpdateWater)
			{
				int randSeeds[9];
				for (int i = 0; i < 9; i++)
					randSeeds[i] = RandomNumber(0u, 500u);

				const glm::ivec3 workGroups = {
					std::ceil(512.0f / 3.0f / 16),
					std::ceil(512.0f / 3.0f / 16),
					400 / 2 / 4 // submesh height / 2 / local_size_z
				};

				for (uint32_t j = 0; j < 2; ++j)
				{
					for (uint32_t i = 0; i < 9; ++i)
					{
						m_VoxelRenderer->SubmitEffect(
							m_WaterMaterial,
							workGroups,
							PushConstBuffer
							{
								randSeeds[i],
								0u, // Model index
								(uint32_t)Empty,
								(uint32_t)Water,
								255u, // Max density
								i,
								j
							}
						);
					}
				}
			}
		}		
		
		VoxelTerrain VoxelPanel::generateVoxelTerrainMesh(
			uint32_t seed, uint32_t frequency, uint32_t octaves,
			uint32_t width, uint32_t height, uint32_t depth
		)
		{
			const double fx = ((double)frequency / (double)width);
			const double fy = ((double)frequency / (double)height);

			VoxelTerrain result;
			result.Terrain.Width = width;
			result.Terrain.Height = height;
			result.Terrain.Depth = depth;
			result.Terrain.VoxelSize = sc_TerrainVoxelSize;

			result.Terrain.ColorIndices.resize(width * height * depth);
			result.WaterMap.resize(width * height * depth);
			memset(result.WaterMap.data(), 0, result.WaterMap.size());

			result.TerrainHeightmap.resize(width * depth);
			for (uint32_t x = 0; x < width; ++x)
			{
				for (uint32_t z = 0; z < depth; ++z)
				{
					const double xDouble = x;
					const double zDouble = z;
					const double val = Perlin::Octave2D(xDouble * fx, zDouble * fy, octaves);
					const uint32_t genHeight = (val / 2.0) * height;

					for (uint32_t y = 0; y < genHeight; y ++)
					{
						result.Terrain.ColorIndices[Utils::Index3D(x, y, z, width, height)] = Grass;
					}
		
					result.TerrainHeightmap[Utils::Index2D(x, z, depth)] = genHeight;

					for (int32_t y = genHeight; y < 50; ++y)
					{
						const uint32_t waterIndex = Utils::Index3D(x, y, z, width, height);
						result.Terrain.ColorIndices[waterIndex] = Water; // Water
						result.WaterMap[waterIndex] = 125; // Half max density
					}
					
					//const uint32_t waterIndex = Utils::Index3D(x, 150, z, width, height);
					//result.Terrain[waterIndex] = Water; // Water
					//result.WaterMap[waterIndex] = 125; // Half max density
				}
			}
			result.Terrain.Compress(16);
			return result;
		}
	}
}
