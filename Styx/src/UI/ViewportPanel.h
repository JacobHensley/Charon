#pragma once
#include "Charon/Core/Core.h"
#include "Charon/Graphics/Camera.h"
#include <imgui/imgui.h>
#include <glm/glm.hpp>

namespace Charon {

	class ViewportPanel
	{
	public:
		ViewportPanel();

	public:
		void Render(Ref<Camera> camera);

		glm::vec2 GetMouseNDC();

		const glm::vec2& GetSize() const { return m_Size; }
		const glm::vec2& GetPosition() const { return m_Position; }

		bool IsHovered() const { return m_Hovered; }
		bool IsFocused() const { return m_Focused; }

	private:
		glm::vec2 m_Size;
		glm::vec2 m_LastSize;
		glm::vec2 m_Position;

		bool m_Hovered;
		bool m_Focused;
	};

}