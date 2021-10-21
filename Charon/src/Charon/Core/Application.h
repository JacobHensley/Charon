#pragma once
#include "Charon/ImGui/ImGuiLayer.h"
#include "Charon/Graphics/Window.h"
#include "Charon/Graphics/VulkanInstance.h"
#include "Charon/Graphics/VulkanDevice.h"
#include "Charon/Graphics/SwapChain.h"
#include "Charon/Graphics/Renderer.h"
#include "Charon/Core/Layer.h"

namespace Charon {

	class Application
	{
	public:
		Application(const std::string name);
		~Application();

	public:
		void Run();

		inline void AddLayer(Ref<Layer> layer) { m_Layers.push_back(layer); }

		inline Ref<Window> GetWindow() { return m_Window; }
		inline Ref<Renderer> GetRenderer() { return m_Renderer; }

		inline Ref<VulkanInstance> GetVulkanInstance() { return m_VulkanInstance; }
		inline Ref<VulkanDevice> GetVulkanDevice() { return m_Device; }
		inline Ref<SwapChain> GetVulkanSwapChain() { return m_SwapChain; }

		inline static Application& GetApp() { return *s_Instance; }

	private:
		void Init();
		void OnUpdate();
		void OnRender();
		void OnImGUIRender();

	private:
		std::string m_Name;
		Ref<Window> m_Window;

		std::vector<Ref<Layer>> m_Layers;
		Ref<ImGuiLayer> m_ImGUILayer;

		Ref<Renderer> m_Renderer;

		Ref<VulkanInstance> m_VulkanInstance;
		Ref<VulkanDevice> m_Device;
		Ref<SwapChain> m_SwapChain;

	private:
		static Application* s_Instance;
	};

}