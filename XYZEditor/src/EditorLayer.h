#pragma once

#include <XYZ.h>


#include "Editor/EditorCamera.h"
#include "Editor/EditorManager.h"
#include "Editor/EditorData.h"

#include "Editor/Panels/ScenePanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/ImGuiStylePanel.h"
#include "Editor/Panels/SceneHierarchyPanel.h"
#include "Editor/Panels/PluginPanel.h"
#include "Editor/Panels/VoxelPanel.h"

#include "Editor/ParticleEditorGPU/ParticleEditorGPU.h"
#include "Editor/Asset/AssetManagerViewPanel.h"
#include "Editor/Asset/AssetBrowser.h"
#include "Editor/Script/ScriptPanel.h"
#include "Editor/SkinningEditor/SkinningEditor.h"
#include "Editor/Animation/AnimationEditor.h"
#include "Editor/Logger/EditorConsolePanel.h"
#include "Editor/Logger/EditorLogger.h"


#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/type_ptr.hpp>



namespace XYZ {
	namespace Editor {

		
		class EditorLayer : public Layer
		{
		public:
			EditorLayer();
			virtual ~EditorLayer() override;

			virtual void OnAttach() override;
			virtual void OnDetach() override;
			virtual void OnUpdate(Timestep ts) override;
			virtual void OnEvent(Event& event) override;
			virtual void OnImGuiRender() override;

			static const EditorData& GetData() { return s_Data; }
		private:
			bool onMouseButtonPress(MouseButtonPressEvent& event);
			bool onMouseButtonRelease(MouseButtonReleaseEvent& event);
			bool onWindowResize(WindowResizeEvent& event);
			bool onKeyPress(KeyPressedEvent& event);
			bool onKeyRelease(KeyReleasedEvent& event);

			void renderOverlay();
			void renderColliders();
			void renderCameras();
			void renderLights();
			void renderSelected();
	
			void createOverlayPipelines();

			std::pair<glm::vec3, glm::vec3> cameraToAABB(const TransformComponent& transform, const SceneCamera& camera) const;
		private:
			void displayStats();

		private:
			// Overlay rendering
			Ref<PrimaryRenderCommandBuffer>	m_CommandBuffer;

			EditorCamera*				m_EditorCamera = nullptr;

			bool m_ShowColliders = true;
			bool m_ShowCameras = true;
			bool m_ShowLights = true;
			/////////////////////
		private:
			SceneEntity					m_SelectedEntity;
			EditorManager				m_EditorManager;

			struct GPUTimeQueries
			{
				uint32_t GPUTime = 0;

				static constexpr uint32_t Count() { return sizeof(GPUTimeQueries) / sizeof(uint32_t); }
			};
			GPUTimeQueries m_GPUTimeQueries;

			inline static EditorData s_Data;
		};
	}
}