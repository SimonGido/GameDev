#include  "stdafx.h"
#include "EditorCamera.h"

#include "XYZ/Core/Input.h"
#include "XYZ/Core/KeyCodes.h"
#include "XYZ/Core/MouseCodes.h"


#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace XYZ {
	namespace Editor {
		EditorCamera::EditorCamera()
			: Camera(glm::perspective(glm::radians(m_FOV), m_AspectRatio, m_NearClip, m_FarClip))
		{
			updateView();
			updateProjection();
		}
		EditorCamera::EditorCamera(float fov, float aspectRatio, float nearClip, float farClip)
			: m_FOV(fov), m_AspectRatio(aspectRatio), m_NearClip(nearClip), m_FarClip(farClip), Camera(glm::perspective(glm::radians(fov), aspectRatio, nearClip, farClip))
		{
			updateView();
			updateProjection();
		}

		Math::Frustum EditorCamera::CreateFrustum() const
		{
			Math::Frustum     frustum;
			const float halfVSide = m_FarClip * tanf(glm::radians(m_FOV));
			const float halfHSide = halfVSide * m_AspectRatio;

			const glm::vec3 frontDir = GetForwardDirection();
			const glm::vec3 rightDir = GetRightDirection();
			const glm::vec3 upDir = GetUpDirection();
			const glm::vec3 frontMultFar = m_FarClip * frontDir;

			frustum.NearFace	= { m_Position + m_NearClip * frontDir, frontDir };
			frustum.FarFace		= { m_Position + frontMultFar, -frontDir };
			frustum.RightFace	= { m_Position, glm::cross(frontMultFar - rightDir * halfHSide, upDir) };
			frustum.LeftFace	= { m_Position, glm::cross(upDir, frontMultFar + rightDir * halfHSide) };
			frustum.TopFace		= { m_Position, glm::cross(rightDir, frontMultFar - upDir * halfVSide) };
			frustum.BottomFace	= { m_Position, glm::cross(frontMultFar + upDir * halfVSide, rightDir) };
			return frustum;
		}

		void EditorCamera::updateProjection()
		{
			m_AspectRatio = m_ViewportWidth / m_ViewportHeight;
			if (std::isnan(m_AspectRatio))
				return;
			m_ProjectionMatrix = glm::perspective(glm::radians(m_FOV), m_AspectRatio, m_NearClip, m_FarClip);
		}

		void EditorCamera::updateView()
		{
			if (m_LockOrtho)
				m_Yaw = m_Pitch = 0.0f; // Lock the camera's rotation

			m_Position = calculatePosition();
			const glm::quat orientation = GetOrientation();
			m_ViewMatrix = glm::translate(glm::mat4(1.0f), m_Position) * glm::toMat4(orientation);
			m_ViewMatrix = glm::inverse(m_ViewMatrix);
		}

		std::pair<float, float> EditorCamera::panSpeed() const
		{
			const float x = std::min(m_ViewportWidth / 1000.0f, 2.4f); // max = 2.4f
			float xFactor = 0.0366f * (x * x) - 0.1778f * x + 0.3021f;

			const float y = std::min(m_ViewportHeight / 1000.0f, 2.4f); // max = 2.4f
			float yFactor = 0.0366f * (y * y) - 0.1778f * y + 0.3021f;

			return { xFactor, yFactor };
		}

		float EditorCamera::rotationSpeed() const
		{
			return 0.8f;
		}

		float EditorCamera::zoomSpeed() const
		{
			float distance = m_Distance * 0.2f;
			distance = std::max(distance, 0.0f);
			float speed = distance * distance;
			speed = std::min(speed, 100.0f); // max speed = 100
			return speed;
		}

		void EditorCamera::OnUpdate(Timestep ts)
		{
			if (Input::IsKeyPressed(KeyCode::KEY_LEFT_CONTROL))
			{
				const glm::vec2& mouse{ Input::GetMouseX(), Input::GetMouseY() };
				const glm::vec2 delta = (mouse - m_InitialMousePosition) * 0.003f;
				m_InitialMousePosition = mouse;

				if (Input::IsMouseButtonPressed(MouseCode::MOUSE_BUTTON_MIDDLE))
					mousePan(delta);
				else if (Input::IsMouseButtonPressed(MouseCode::MOUSE_BUTTON_LEFT))
					mouseRotate(delta);
				else if (Input::IsMouseButtonPressed(MouseCode::MOUSE_BUTTON_RIGHT))
					mouseZoom(delta.y);
			}

			updateView();
		}

		void EditorCamera::OnEvent(Event& e)
		{
			EventDispatcher dispatcher(e);
			dispatcher.Dispatch<MouseScrollEvent>(Hook(&EditorCamera::onMouseScroll, this));
		}

		bool EditorCamera::onMouseScroll(MouseScrollEvent& e)
		{
			const float delta = (float)e.GetOffsetY() * 0.1f;
			mouseZoom(delta);
			updateView();
			return false;
		}

		void EditorCamera::mousePan(const glm::vec2& delta)
		{
			auto [xSpeed, ySpeed] = panSpeed();
			m_FocalPoint += -GetRightDirection() * delta.x * xSpeed * m_Distance;
			m_FocalPoint += GetUpDirection() * delta.y * ySpeed * m_Distance;
		}

		void EditorCamera::mouseRotate(const glm::vec2& delta)
		{
			const float yawSign = GetUpDirection().y < 0 ? -1.0f : 1.0f;
			m_Yaw += yawSign * delta.x * rotationSpeed();
			m_Pitch += delta.y * rotationSpeed();
		}

		void EditorCamera::mouseZoom(float delta)
		{
			m_Distance -= delta * zoomSpeed();
			if (m_Distance < 1.0f)
			{
				m_FocalPoint += GetForwardDirection();
				m_Distance = 1.0f;
			}
		}

		glm::vec3 EditorCamera::GetUpDirection() const
		{
			return glm::rotate(GetOrientation(), glm::vec3(0.0f, 1.0f, 0.0f));
		}

		glm::vec3 EditorCamera::GetRightDirection() const
		{
			return glm::rotate(GetOrientation(), glm::vec3(1.0f, 0.0f, 0.0f));
		}

		glm::vec3 EditorCamera::GetForwardDirection() const
		{
			return glm::rotate(GetOrientation(), glm::vec3(0.0f, 0.0f, -1.0f));
		}

		glm::vec3 EditorCamera::calculatePosition() const
		{
			return m_FocalPoint - GetForwardDirection() * m_Distance;
		}

		glm::quat EditorCamera::GetOrientation() const
		{
			return glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f));
		}
	}
}