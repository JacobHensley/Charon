#include "pch.h"
#include "ViewerLayer.h"
#include "Charon/Core/Core.h"
#include "Charon/Core/Application.h"
#include "Charon/Graphics/Mesh.h"
#include "Charon/Graphics/Renderer.h"
#include "Charon/Scene/Components.h"
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
		m_Scene = CreateRef<Scene>();
		m_MeshHandle = AssetManager::Load<Mesh>("assets/models/ObjectParentTest.gltf");
		m_SceneObject = m_Scene->CreateObject("Test Object");
		m_SceneObject.AddComponent<MeshComponent>(m_MeshHandle);
		m_Camera = CreateRef<Camera>(glm::perspectiveFov(glm::radians(45.0f), 1280.0f, 720.0f, 0.1f, 100.0f));
		m_MeshTransform = glm::mat4(1.0f);
	}

	void ViewerLayer::OnUpdate()
	{
		m_Camera->Update();
		m_Scene->Update();
	}

	void ViewerLayer::OnRender()
	{
		m_Scene->Render(m_Camera);

		Ref<Renderer> renderer = Application::GetApp().GetRenderer();

		renderer->BeginScene(m_Camera);

		renderer->BeginRenderPass(renderer->GetFramebuffer());
		renderer->SubmitMesh(m_SceneObject.GetComponent<MeshComponent>().GetMesh(), m_SceneObject.GetComponent<TransformComponent>().GetTransform());
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