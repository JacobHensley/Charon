#pragma once
#include <glm/glm.hpp>

namespace Charon {

	class Camera
	{
	public:
		Camera(const glm::mat4& projectionMatrix);

	public:
		void Update();
		void Reset();

		const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
		void SetProjectionMatrix(const glm::mat4& matrix) { m_ProjectionMatrix = matrix; }

		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		const glm::mat4 GetInverseViewMatrix() const { return glm::inverse(m_ViewMatrix); }
		const glm::mat4 GetViewProjection() const { return m_ProjectionMatrix * m_ViewMatrix; }
		const glm::mat4 GetInverseVP() const { return glm::inverse(m_ProjectionMatrix * m_ViewMatrix); }

		const glm::vec3& GetPosition() const { return m_Position; }

	private:
		void MousePan(const glm::vec2& delta);
		void MouseRotate(const glm::vec2& delta);
		void MouseZoom(float delta);

		glm::vec3 GetUpDirection();
		glm::vec3 GetRightDirection();
		glm::vec3 GetForwardDirection();

		glm::vec3 CalculatePosition();
		glm::quat GetOrientation();

	private:
		glm::mat4 m_ProjectionMatrix, m_ViewMatrix;
		glm::vec3 m_Position, m_Rotation, m_FocalPoint;

		bool m_Panning, m_Rotating;
		glm::vec2 m_InitialMousePosition;
		glm::vec3 m_InitialFocalPoint, m_InitialRotation;

		float m_Distance;
		float m_PanSpeed, m_RotationSpeed, m_ZoomSpeed;

		float m_Pitch, m_Yaw;
	};

}
