#include "pch.h"
#include "ViewerLayer.h"
#include "Charon/Core/Core.h"
#include "Charon/Core/Application.h"
#include "Charon/Graphics/Mesh.h"
#include "Charon/Graphics/Renderer.h"
#include "Charon/ImGui/imgui_impl_vulkan_with_textures.h"
#include "imgui/imgui.h"

namespace Charon {

	ViewerLayer::ViewerLayer()
		: Layer("Viewer")
	{
		Init();
	}

	void ViewerLayer::Init()
	{
		m_Camera = CreateRef<Camera>(glm::perspectiveFov(glm::radians(45.0f), 1280.0f, 720.0f, 0.1f, 100.0f));
		m_MeshHandle = AssetManager::Load<Mesh>("assets/models/ObjectParentTest.gltf");

		m_MeshTransform = glm::mat4(1.0f);
	}

	void ViewerLayer::OnUpdate()
	{
		m_Camera->Update();
	}

	void ViewerLayer::OnRender()
	{
		Ref<Renderer> renderer = Application::GetApp().GetRenderer();

		renderer->BeginScene(m_Camera);

		renderer->BeginRenderPass(renderer->GetFramebuffer());
		renderer->SubmitMesh(AssetManager::Get<Mesh>(m_MeshHandle), m_MeshTransform);
		renderer->Render();
		renderer->EndRenderPass();

		renderer->BeginRenderPass();
		renderer->RenderUI();
		renderer->EndRenderPass();

		renderer->EndScene();
	}

	void ViewerLayer::OnImGUIRender()
	{
		Ref<Renderer> renderer = Application::GetApp().GetRenderer();

		ImGui::Begin("Viewport");

		auto& descriptorInfo = renderer->GetFramebuffer()->GetImage(0)->GetDescriptorImageInfo();
		ImTextureID imTex = ImGui_ImplVulkan_AddTexture(descriptorInfo.sampler, descriptorInfo.imageView, descriptorInfo.imageLayout);

		const auto& fbSpec = renderer->GetFramebuffer()->GetSpecification();
		float width = ImGui::GetContentRegionAvail().x;
		float aspect = (float)fbSpec.Height / (float)fbSpec.Width;

		ImGui::Image(imTex, { width, width * aspect }, ImVec2(0, 1), ImVec2(1, 0));

		ImGui::End();

		ImGui::ShowDemoWindow();
	}

}