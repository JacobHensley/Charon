#include "pch.h"
#include "Application.h"
#include "Charon/Graphics/VulkanAllocator.h"
#include "Charon/Asset/AssetManager.h"
#include <imgui.h>

namespace Charon {

	Application* Application::s_Instance = nullptr;

	Application::Application(const std::string name)
		: m_Name(name)
	{
		Init();
		CR_LOG_INFO("Initialized Application");
	}

	Application::~Application()
	{
		for (auto& layer : m_Layers)
		{
			layer.reset();
		}

		m_Renderer.reset();
		m_ImGUILayer.reset();

		// Vulkan shutdown
		m_SwapChain.reset();
		AssetManager::Clear();
		VulkanAllocator::Shutdown();
		m_Device.reset();
		m_Window.reset();
		m_VulkanInstance.reset();
	}

	void Application::Init()
	{
		CR_ASSERT(!s_Instance, "Instance of Application already exists");
		s_Instance = this;

		Log::Init();
		
		// Vulkan initialization
		m_Window = CreateRef<Window>(m_Name, 1280, 720);
		m_VulkanInstance = CreateRef<VulkanInstance>(m_Name);
		m_Window->InitVulkanSurface();
		m_Device = CreateRef<VulkanDevice>();
		m_SwapChain = CreateRef<SwapChain>();
		VulkanAllocator::Init(m_Device);

		m_Renderer = CreateRef<Renderer>();
		m_ImGUILayer = CreateRef<ImGuiLayer>();
	}

	void Application::OnUpdate()
	{
		m_Window->OnUpdate();

		for (auto& layer : m_Layers)
		{
			layer->OnUpdate();
		}
	}

	void Application::OnRender()
	{
		for (auto& layer : m_Layers)
		{
			layer->OnRender();
		}
	}

	void Application::OnImGUIRender()
	{
		m_ImGUILayer->BeginFrame();

		m_Renderer->OnImGuiRender();
		for (auto& layer : m_Layers)
		{
			layer->OnImGUIRender();
		}

		m_ImGUILayer->EndFrame();

		// Render UI
		{
			auto renderer = Application::GetApp().GetRenderer();
			renderer->BeginRenderPass();
			renderer->RenderUI();
			renderer->EndRenderPass();
		}
	}

	void Application::Run()
	{
		float lastTime = m_GlobalTime;
		while (!m_Window->IsClosed())
		{
			m_GlobalTime = (float)glfwGetTime();
			m_DeltaTime = m_GlobalTime - lastTime;
			m_DeltaTime = glm::min(m_DeltaTime, 1000.0f / 30.0f);
			lastTime = m_GlobalTime;

			OnUpdate();
			
			m_SwapChain->BeginFrame();
			m_Renderer->BeginFrame();

			OnRender();
			OnImGUIRender();

			m_Renderer->EndFrame();
			m_SwapChain->Present();
		}

		vkDeviceWaitIdle(m_Device->GetLogicalDevice());
	}

}