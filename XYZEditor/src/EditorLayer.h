#pragma once

#include <XYZ.h>

#include "Panels/SceneHierarchyPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/GraphPanel.h"
#include "InspectorLayout/EntityInspectorLayout.h"
#include "InspectorLayout/AnimatorInspectorLayout.h"
#include "GraphLayout/AnimatorGraphLayout.h"

#include "Tools/EditorCamera.h"


#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/type_ptr.hpp>


namespace XYZ {

	class EditorLayer : public Layer
	{
	public:
		virtual ~EditorLayer();

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate(Timestep ts) override;
		virtual void OnEvent(Event& event) override;
		virtual void OnInGuiRender(Timestep ts) override;

	private:
		bool onWindowResized(WindowResizeEvent& event);
		bool onMouseButtonPress(MouseButtonPressEvent& event);
		bool onKeyPress(KeyPressedEvent& event);
		bool onKeyRelease(KeyReleasedEvent& event);

		void onResizeSceneWindow(const glm::vec2& size);
		void onNodePanelConnectionCreated(uint32_t startNode, uint32_t endNode);
		void onNodePanelConnectionDestroyed(uint32_t startNode, uint32_t endNode);

	private:
		SceneHierarchyPanel m_SceneHierarchyPanel;
		
		InspectorPanel m_InspectorPanel;
		EntityInspectorLayout m_EntityInspectorLayout;
		AnimatorInspectorLayout m_AnimatorInspectorLayout;

		GraphPanel m_GraphPanel;
		AnimatorGraphLayout m_AnimatorGraphLayout;
		Graph m_NodeGraph;


		Entity m_SelectedEntity;
		glm::vec2 m_StartMousePos;
		bool m_ScalingEntity = false;
		bool m_MovingEntity = false;
		bool m_RotatingEntity = false;

		TransformComponent* m_ModifiedTransform = nullptr;
		glm::vec3 m_Scale;
		glm::vec3 m_Translation;
		glm::quat m_Orientation;


		EditorCamera m_EditorCamera;
		Ref<Scene> m_Scene;
		StateMachine m_StateMachine;

		Ref<FrameBuffer> m_FBO;
		AssetManager m_AssetManager;
		std::vector<Entity> m_StoredEntitiesWithScript;
		bool m_Lock = false;
		float m_H = 0.0f;
		std::string m_Trala;
	private:	
		Entity m_TestEntity;
		Entity m_TestEntity2;
		Entity m_TextEntity;

		SpriteRenderer* m_SpriteRenderer;
		TransformComponent* m_Transform;
		Animation* m_Animation;
		Animation* m_RunAnimation;

		glm::vec3 m_Position = { 0,0,0 };
		glm::vec3 m_Rotation = { 0,0,0 };

		glm::vec4 m_Color = { 0,0,0,0 };
		glm::vec4 m_Pallete = { 0,1,0,1 };
		float m_TestValue = 0.0f;
		bool m_CheckboxVal = false;
		bool m_ActiveWindow = false;
		bool m_MenuOpen = false;
		bool m_PopupOpen = false;
		std::string m_Text = "0";
		bool m_Modified = false;
	
		


		Ref<Material> m_Material;
		Ref<Texture2D> m_CharacterTexture;

		Ref<SubTexture2D> m_CharacterSubTexture;
		Ref<SubTexture2D> m_CharacterSubTexture2;
		Ref<SubTexture2D> m_CharacterSubTexture3;

		Ref<SubTexture2D> m_CheckboxSubTexture;


		
	};
}