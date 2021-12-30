#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "imgui/imgui.h"

namespace Charon {

	struct ImGradientMark
	{
		glm::vec3 color;
		float position;
	};

	class ImGradient
	{
	public:
		ImGradient();
		~ImGradient();

		void addMark(float position, ImColor const color);
		void removeMark(ImGradientMark* mark);
		void getColorAt(float position, glm::vec4& color) const;
		void computeColorAt(float position, float* color) const;
		void refreshCache();

		std::vector<ImGradientMark*>& getMarks() { return m_marks; }
	private:
		std::vector<ImGradientMark*> m_marks;
		float m_cachedValues[256 * 3];

	};

	void DrawGradientBar(ImGradient* gradient, const ImVec2& bar_pos, float maxWidth, float height);
	void DrawGradientMarks(ImGradient* gradient,
		ImGradientMark*& draggingMark,
		ImGradientMark*& selectedMark,
		struct ImVec2 const& bar_pos,
		float maxWidth,
		float height);

	bool GradientButton(ImGradient* gradient);

	bool GradientEditor(ImGradient* gradient,
		ImGradientMark*& draggingMark,
		ImGradientMark*& selectedMark, bool& dragging);
}

