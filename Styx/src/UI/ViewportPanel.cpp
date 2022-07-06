#include "ViewportPanel.h"
#include "Charon/Graphics/SceneRenderer.h"
#include "Charon/ImGui/imgui_impl_vulkan_with_textures.h"
#include <glm/gtc/type_ptr.hpp>

namespace Charon {

	ViewportPanel::ViewportPanel()
	{
	}

	bool ViewportPanel::Render(Ref<Camera> camera)
	{
		// Set window settings
		ImGuiIO io = ImGui::GetIO();
		io.ConfigWindowsMoveFromTitleBarOnly = true;
		bool open = true;
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::Begin("Editor", &open, flags);

		// Set data
		ImVec2 size = ImGui::GetContentRegionAvail();
		m_Size = { size.x, size.y };

		ImVec2 position = ImGui::GetWindowPos();
		m_Position = { position.x, position.y };

		ImVec2 windowOffset = ImGui::GetCursorPos();
		m_Position.x += windowOffset.x;
		m_Position.y += windowOffset.y;

		m_Hovered = ImGui::IsWindowHovered();
		m_Focused = ImGui::IsWindowFocused();

		// Draw scene
		auto& descriptorInfo = SceneRenderer::GetFinalBuffer()->GetImage(0)->GetDescriptorImageInfo();
		ImTextureID textureID = ImGui_ImplVulkan_AddTexture(descriptorInfo.sampler, descriptorInfo.imageView, descriptorInfo.imageLayout);

		ImGui::Image((void*)textureID, ImVec2(m_Size.x, m_Size.y), ImVec2::ImVec2(0, 1), ImVec2::ImVec2(1, 0));
		bool resized = SceneRenderer::SetViewportSize((uint32_t)m_Size.x, (uint32_t)m_Size.y);

		// Resize window
		if (m_Size != m_LastSize)
		{
			camera->SetProjectionMatrix(glm::perspectiveFov(glm::radians(45.0f), m_Size.x, m_Size.y, 0.1f, 100.0f));
			m_LastSize = m_Size;

#if 0
			Ref<Framebuffer> framebuffer = SceneRenderer::GetFinalBuffer();
			FramebufferSpecification spec = framebuffer->GetSpecification();
			spec.Width = m_Size.x;
			spec.Height = m_Size.y;
			framebuffer = CreateRef<Framebuffer>(spec);
			
			Ref<Framebuffer> framebuffer1 = SceneRenderer::GetFinalBuffer();
#endif
		}

		ImGui::End();
		ImGui::PopStyleVar();

		return resized;
	}

	glm::vec2 ViewportPanel::GetMouseNDC()
	{
		auto [mouseX, mouseY] = ImGui::GetMousePos();
		mouseX -= m_Position.x;
		mouseY -= m_Position.y;

		glm::vec2 NDC = { (mouseX / m_Size.x) * 2.0f - 1.0f, ((mouseY / m_Size.y) * 2.0f - 1.0f) * -1.0f };

		return NDC;
	}

}