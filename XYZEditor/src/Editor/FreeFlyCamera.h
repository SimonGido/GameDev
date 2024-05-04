#pragma once
#include "XYZ/Renderer/Camera.h"

#include "XYZ/Core/Timestep.h"
#include "XYZ/Event/Event.h"
#include "XYZ/Event/InputEvent.h"
#include "XYZ/Utils/Math/Math.h"

#include <glm/glm.hpp>

namespace XYZ {
	namespace Editor {

		class FreeFlyCamera : public Camera
		{
		public:
			FreeFlyCamera();
			FreeFlyCamera(float fov, float aspectRatio, float nearClip, float farClip);

			void OnUpdate(Timestep ts);
			void OnEvent(Event& e);
			inline void  SetViewportSize(float width, float height) { m_ViewportWidth = width; m_ViewportHeight = height; updateProjection(); }

			const glm::vec3& GetPosition() const { return m_Position; }
			void SetPosition(const glm::vec3& position) { m_Position = position; updateView();}
			const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
			glm::mat4 GetViewProjection() const { return m_ProjectionMatrix * m_ViewMatrix; }

			Math::Frustum CreateFrustum() const;

			glm::vec3 GetUpDirection() const;
			glm::vec3 GetRightDirection() const;
			glm::vec3 GetForwardDirection() const;
			glm::quat GetOrientation() const;

		private:
			void updateProjection();
			void updateView();

			void mouseRotate(const glm::vec2& delta);
			std::pair<float, float> panSpeed() const;
		private:
			float m_FOV = 45.0f, m_AspectRatio = 1.778f, m_NearClip = 0.1f, m_FarClip = -1000.0f;
			float m_Pitch = 0.0f, m_Yaw = 0.0f;

			float m_ViewportWidth = 1280, m_ViewportHeight = 720;

			glm::vec3 m_Position = glm::vec3(0.0f);

			glm::mat4 m_ViewMatrix = glm::mat4(1.0f);
		
			glm::vec2 m_InitialMousePosition = { 0.0f, 0.0f };
		};
	}
}