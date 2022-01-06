#include "pch.h"
#include "Camera.h"
#include "Charon/Core/Application.h"
#include "Charon/Input/Input.h"
#include "Charon/Input/KeyCodes.h"
#include <glm/gtx/quaternion.hpp>

#define PI 3.14159f

namespace Charon {

	Camera::Camera(const glm::mat4& projectionMatrix)
		: m_ProjectionMatrix(projectionMatrix)
	{
		Reset();
	}

	void Camera::Update()
	{
		const glm::vec2& mouse{ Input::GetMouseX(), Input::GetMouseY() };
		glm::vec2 delta = mouse - m_InitialMousePosition;
		m_InitialMousePosition = mouse;

		if (Input::IsKeyPressed(KEY_LEFT_SHIFT) && Input::IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE))
		{
			MousePan(delta);
		}
		else if (Input::IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE))
		{
			MouseRotate(delta);
		}

		Ref<Window> window = Application::GetApp().GetWindow();

		if (window->GetIsMouseScrolling())
		{
			MouseZoom(window->GetMouseScrollWheel());
		}
		
		m_Position = CalculatePosition();

		glm::quat orientation = GetOrientation();
		m_Rotation = glm::eulerAngles(orientation) * (180.0f / (float)PI);
		m_ViewMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 1)) * glm::toMat4(glm::conjugate(orientation)) * glm::translate(glm::mat4(1.0f), -m_Position);
	}

	void Camera::Reset()
	{
		m_PanSpeed = 0.001f;
		m_RotationSpeed = 0.001f;
		m_ZoomSpeed = 2.0f;

		m_Position = { -10, 10, 10 };
		m_Rotation = glm::vec3(90.0f, 0.0f, 0.0f);

		m_FocalPoint = glm::vec3(0.0f);
		m_Distance = glm::distance(m_Position, m_FocalPoint);

		m_Yaw = 3.0f * (float)PI / 4.0f;
		m_Pitch = PI / 4.0f;
	}

	void Camera::MousePan(const glm::vec2& delta)
	{
		m_FocalPoint += -GetRightDirection() * delta.x * m_PanSpeed * m_Distance;
		m_FocalPoint += GetUpDirection() * delta.y * m_PanSpeed * m_Distance;
	}

	void Camera::MouseRotate(const glm::vec2& delta)
	{
		float yawSign = GetUpDirection().y < 0 ? -1.0f : 1.0f;
		m_Yaw += yawSign * delta.x * m_RotationSpeed;
		m_Pitch += delta.y * m_RotationSpeed;
	}

	void Camera::MouseZoom(float delta)
	{
		m_Distance -= delta * m_ZoomSpeed;
	}

	glm::vec3 Camera::GetUpDirection()
	{
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 1.0f, 0.0f));
	}

	glm::vec3 Camera::GetRightDirection()
	{
		return glm::rotate(GetOrientation(), glm::vec3(1.0f, 0.0f, 0.0f));
	}

	glm::vec3 Camera::GetForwardDirection()
	{
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 0.0f, -1.0f));
	}

	glm::vec3 Camera::CalculatePosition()
	{
		return m_FocalPoint - GetForwardDirection() * m_Distance;
	}

	glm::quat Camera::GetOrientation()
	{
		return glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f));
	}
}