#include "stdafx.h"
#include "FreeFlyCamera.h"


#include "XYZ/Core/Input.h"
#include "XYZ/Core/KeyCodes.h"
#include "XYZ/Core/MouseCodes.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace XYZ {
	namespace Editor {
		FreeFlyCamera::FreeFlyCamera()
			: Camera(glm::perspective(glm::radians(m_FOV), m_AspectRatio, m_NearClip, m_FarClip))
		{
			updateView();
			updateProjection();
		}
		FreeFlyCamera::FreeFlyCamera(float fov, float aspectRatio, float nearClip, float farClip)
			: m_FOV(fov), m_AspectRatio(aspectRatio), m_NearClip(nearClip), m_FarClip(farClip), Camera(glm::perspective(glm::radians(fov), aspectRatio, nearClip, farClip))
		{
			updateView();
			updateProjection();
		}
		void FreeFlyCamera::OnUpdate(Timestep ts)
		{
			float cameraSpeed = 50.0f * ts; // Adjust this value as needed
			float rotationSensitivity = 1.5f * ts;

			const glm::vec3 forwardDir = GetForwardDirection();
			const glm::vec3 upDir = GetUpDirection();
			const glm::quat orientation = GetOrientation();

			if (Input::IsKeyPressed(KeyCode::KEY_LEFT_SHIFT))
				cameraSpeed *= 3;


			if (Input::IsKeyPressed(KeyCode::KEY_A))
				m_Yaw -= rotationSensitivity;
			if (Input::IsKeyPressed(KeyCode::KEY_D))
				m_Yaw += rotationSensitivity;
			if (Input::IsKeyPressed(KeyCode::KEY_W))
				m_Pitch -= rotationSensitivity;
			if (Input::IsKeyPressed(KeyCode::KEY_S))
				m_Pitch += rotationSensitivity;

			if (Input::IsKeyPressed(KeyCode::KEY_UP))
				m_Position += cameraSpeed * forwardDir;
			if (Input::IsKeyPressed(KeyCode::KEY_DOWN))
				m_Position -= cameraSpeed * forwardDir;
			if (Input::IsKeyPressed(KeyCode::KEY_LEFT))
				m_Position -= glm::normalize(glm::cross(forwardDir, upDir)) * cameraSpeed;
			if (Input::IsKeyPressed(KeyCode::KEY_RIGHT))
				m_Position += glm::normalize(glm::cross(forwardDir, upDir)) * cameraSpeed;

			// Recalculate view matrix
			updateView();
		}
		void FreeFlyCamera::OnEvent(Event& e)
		{
		}
		Math::Frustum FreeFlyCamera::CreateFrustum() const
		{
			Math::Frustum     frustum;
			const float halfVSide = m_FarClip * tanf(glm::radians(m_FOV));
			const float halfHSide = halfVSide * m_AspectRatio;

			const glm::vec3 frontDir = GetForwardDirection();
			const glm::vec3 rightDir = GetRightDirection();
			const glm::vec3 upDir = GetUpDirection();
			const glm::vec3 frontMultFar = m_FarClip * frontDir;

			frustum.NearFace = { m_Position + m_NearClip * frontDir, frontDir };
			frustum.FarFace = { m_Position + frontMultFar, -frontDir };
			frustum.RightFace = { m_Position, glm::cross(frontMultFar - rightDir * halfHSide, upDir) };
			frustum.LeftFace = { m_Position, glm::cross(upDir, frontMultFar + rightDir * halfHSide) };
			frustum.TopFace = { m_Position, glm::cross(rightDir, frontMultFar - upDir * halfVSide) };
			frustum.BottomFace = { m_Position, glm::cross(frontMultFar + upDir * halfVSide, rightDir) };
			return frustum;
		}
		glm::vec3 FreeFlyCamera::GetUpDirection() const
		{
			return glm::rotate(GetOrientation(), glm::vec3(0.0f, 1.0f, 0.0f));
		}
		glm::vec3 FreeFlyCamera::GetRightDirection() const
		{
			return glm::rotate(GetOrientation(), glm::vec3(1.0f, 0.0f, 0.0f));
		}
		glm::vec3 FreeFlyCamera::GetForwardDirection() const
		{
			return glm::rotate(GetOrientation(), glm::vec3(0.0f, 0.0f, -1.0f));
		}
		glm::quat FreeFlyCamera::GetOrientation() const
		{
			return glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f));
		}
		void FreeFlyCamera::updateProjection()
		{
			m_AspectRatio = m_ViewportWidth / m_ViewportHeight;
			if (std::isnan(m_AspectRatio))
				return;
			m_ProjectionMatrix = glm::perspective(glm::radians(m_FOV), m_AspectRatio, m_NearClip, m_FarClip);
		}
		void FreeFlyCamera::updateView()
		{
			const glm::quat orientation = GetOrientation();
			m_ViewMatrix = glm::translate(glm::mat4(1.0f), m_Position) * glm::toMat4(orientation);
			m_ViewMatrix = glm::inverse(m_ViewMatrix);
		}
		void FreeFlyCamera::mouseRotate(const glm::vec2& delta)
		{
			const float yawSign = GetUpDirection().y < 0 ? -1.0f : 1.0f;
			m_Yaw += yawSign * delta.x * 0.8f;
			m_Pitch += delta.y * 0.8f;
		}
		std::pair<float, float> FreeFlyCamera::panSpeed() const
		{
			const float x = std::min(m_ViewportWidth / 1000.0f, 2.4f); // max = 2.4f
			float xFactor = 0.0366f * (x * x) - 0.1778f * x + 0.3021f;

			const float y = std::min(m_ViewportHeight / 1000.0f, 2.4f); // max = 2.4f
			float yFactor = 0.0366f * (y * y) - 0.1778f * y + 0.3021f;

			return { xFactor, yFactor };
		}
	}
}