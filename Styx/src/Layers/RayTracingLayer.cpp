#include "pch.h"
#include "RayTracingLayer.h"
#include "Charon/Core/Core.h"
#include "Charon/Core/Application.h"
#include "Charon/Graphics/Mesh.h"
#include "Charon/Graphics/SceneRenderer.h"
#include "Charon/Scene/Components.h"
#include "Charon/ImGui/imgui_impl_vulkan_with_textures.h"
#include "imgui/imgui.h"

namespace Charon {

	RayTracingLayer::RayTracingLayer()
		: Layer("RayTracing")
	{
		Init();
	}

	void RayTracingLayer::Init()
	{
		m_Scene = CreateRef<Scene>();
		m_Camera = CreateRef<Camera>(glm::perspectiveFov(glm::radians(45.0f), 1280.0f, 720.0f, 0.1f, 100.0f));
		m_ViewportPanel = CreateRef<ViewportPanel>();

		m_SceneObject = m_Scene->CreateObject("Test Object");
		m_MeshHandle = AssetManager::Load<Mesh>("assets/models/Cornell.gltf");

		AccelerationStructureSpecification spec;
		spec.Mesh = AssetManager::Get<Mesh>(m_MeshHandle);
		spec.Transform = glm::mat4(1.0f);
		m_AccelerationStructure = CreateRef<VulkanAccelerationStructure>(spec);

		m_SceneObject.AddComponent<MeshComponent>(m_MeshHandle);
	}

	void RayTracingLayer::OnUpdate()
	{
		m_Camera->Update();
		m_Scene->Update();
	}

	void RayTracingLayer::OnRender()
	{
		m_Scene->Render(m_Camera);
	}

	void RayTracingLayer::OnImGUIRender()
	{
		m_ViewportPanel->Render(m_Camera);

#if 0
		ImGui::Begin("Viewport");

		auto& descriptorInfo = SceneRenderer::GetFinalBuffer()->GetImage(0)->GetDescriptorImageInfo();
		ImTextureID imTex = ImGui_ImplVulkan_AddTexture(descriptorInfo.sampler, descriptorInfo.imageView, descriptorInfo.imageLayout);

		const auto& fbSpec = SceneRenderer::GetFinalBuffer()->GetSpecification();
		float width = ImGui::GetContentRegionAvail().x;
		float aspect = (float)fbSpec.Height / (float)fbSpec.Width;

		ImGui::Image(imTex, { width, width * aspect }, ImVec2(0, 1), ImVec2(1, 0));

		ImGui::End();

		ImGui::ShowDemoWindow();
#endif
	}

}