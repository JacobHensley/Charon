#pragma once
#include "VulkanDevice.h"
#include "VulkanPipeline.h"
#include <Vulkan/vulkan.h>

namespace Charon {

	struct SwapChainImage
	{
		VkImage Image;
		VkImageView ImageView;
	};

	class SwapChain
	{
	public:
		SwapChain();
		~SwapChain();

	public:
		void BeginFrame();
		void Present();

		inline VkSwapchainKHR GetSwapChainHandle() { return m_SwapChain; }
		inline VkRenderPass GetRenderPass() { return m_RenderPass; }

		inline const std::vector<VkFramebuffer>& GetFramebuffers() { return m_Framebuffers; }
		VkFramebuffer GetCurrentFramebuffer() const { return m_Framebuffers[m_CurrentImageIndex]; }

		inline std::vector<VkCommandBuffer>& GetCommandBuffers() { return m_CommandBuffers; }
		VkCommandBuffer GetCurrentCommandBuffer() const { return m_CommandBuffers[m_CurrentBufferIndex]; }
		VkCommandPool GetCommandPool() const { return m_CommandPool; }

		inline uint32_t GetCurrentBufferIndex() { return m_CurrentBufferIndex; }
		inline uint32_t GetCurrentFrameIndex() { return m_CurrentImageIndex; }
		uint32_t GetFramesInFlight();

		inline uint32_t GetImageCount() { return m_ImageCount; }
		inline uint32_t GetMinImageCount() { return m_MinImageCount; }
		inline VkExtent2D GetExtent() { return m_Extent; }

	private:
		void Init();
		void Destroy();

		void PickDetails();
		void CreateImageViews();
		void CreateFramebuffers();
		void CreateCommandBuffers();
		void CreateSynchronizationObjects();

		void Resize();
		VkResult QueuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore);

	private:
		VkSwapchainKHR m_SwapChain = nullptr;
		VkRenderPass m_RenderPass = nullptr;

		VkCommandPool m_CommandPool = nullptr;
		std::vector<VkCommandBuffer> m_CommandBuffers;

		std::vector<SwapChainImage> m_Images;
		std::vector<VkFramebuffer> m_Framebuffers;

		uint32_t m_CurrentImageIndex = 0;
		uint32_t m_CurrentBufferIndex = 0;

		std::vector<VkSemaphore> m_PresentCompleteSemaphores;
		std::vector<VkSemaphore> m_RenderCompleteSemaphores;
		std::vector<VkFence> m_WaitFences;

		VkSurfaceFormatKHR m_ImageFormat;
		VkPresentModeKHR m_PresentMode;
		VkExtent2D m_Extent;
		uint32_t m_ImageCount;
		uint32_t m_MinImageCount;
	};

}