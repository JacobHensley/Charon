#include "pch.h"
#include "ImGuiLayer.h"
#include "Charon/Core/Application.h"
#include "Charon/Graphics/VulkanTools.h"
#include "Charon/ImGui/imgui_impl_glfw.h"
#include "Charon/ImGui/imgui_impl_vulkan_with_textures.h"

namespace Charon {

    ImGuiLayer::ImGuiLayer()
	{
		Init();
	}

    ImGuiLayer::~ImGuiLayer()
	{
        Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();

        // Vulkan shutdown
        vkDeviceWaitIdle(device->GetLogicalDevice());
        vkDestroyDescriptorPool(device->GetLogicalDevice(), m_DescriptorPool, nullptr);

        // ImGui shutdown
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
	}

	void ImGuiLayer::Init()
	{
		Ref<SwapChain> swapChain = Application::GetApp().GetVulkanSwapChain();
		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();
		Ref<VulkanInstance> instance = Application::GetApp().GetVulkanInstance();
        Ref<Window> window = Application::GetApp().GetWindow();

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows

        ImGui::StyleColorsDark();

        // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        // Create Descriptor Pool
        {
            VkDescriptorPoolSize pool_sizes[] =
            {
                { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
                { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
                { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
            };

            VkDescriptorPoolCreateInfo pool_info = {};
            pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
            pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
            pool_info.pPoolSizes = pool_sizes;

            vkCreateDescriptorPool(device->GetLogicalDevice(), &pool_info, nullptr, &m_DescriptorPool);
        }

        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForVulkan(window->GetWindowHandle(), true);
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = instance->GetInstanceHandle();
        init_info.PhysicalDevice = device->GetPhysicalDevice();
        init_info.Device = device->GetLogicalDevice();
        init_info.QueueFamily = device->GetQueueIndices().GraphicsQueue.value();
        init_info.Queue = device->GetGraphicsQueue();
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = m_DescriptorPool;
        init_info.Allocator = nullptr;
        init_info.MinImageCount = swapChain->GetMinImageCount();
        init_info.ImageCount = swapChain->GetImageCount();
        init_info.CheckVkResultFn = nullptr;
        ImGui_ImplVulkan_Init(&init_info, swapChain->GetRenderPass());

        // Upload Fonts
        {
            // Use any command queue
            VkCommandPool command_pool = swapChain->GetCommandPool();
            VkCommandBuffer command_buffer = swapChain->GetCommandBuffers()[0];

            vkResetCommandPool(device->GetLogicalDevice(), command_pool, 0);

            VkCommandBufferBeginInfo begin_info = {};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            vkBeginCommandBuffer(command_buffer, &begin_info);

            ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

            VkSubmitInfo end_info = {};
            end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            end_info.commandBufferCount = 1;
            end_info.pCommandBuffers = &command_buffer;

            vkEndCommandBuffer(command_buffer);

            vkQueueSubmit(device->GetGraphicsQueue(), 1, &end_info, VK_NULL_HANDLE);
            vkDeviceWaitIdle(device->GetLogicalDevice());

            ImGui_ImplVulkan_DestroyFontUploadObjects();
        } 
	}

	void ImGuiLayer::BeginFrame()
	{
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        bool p_open = true;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
        // because it would be confusing to have two docking targets within each others.
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background and handle the pass-thru hole, so we ask Begin() to not render a background.
        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
        {
            window_flags |= ImGuiWindowFlags_NoBackground;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace", &p_open, window_flags);
        ImGui::PopStyleVar();

        ImGui::PopStyleVar(2);

        // DockSpace
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

        ImGui::End();
	}

	void ImGuiLayer::EndFrame()
	{
        Ref<SwapChain> swapChain = Application::GetApp().GetVulkanSwapChain();
        ImGui::Render();

        ImGuiIO& io = ImGui::GetIO();

        // Update and Render additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
	}

}