#include "pch.h"
#include "SwapChain.h"
#include "Charon/Core/Application.h"
#include "Charon/Graphics/VulkanTools.h"
#include <glm/glm.hpp>

namespace Charon {

	static const int MAX_FRAMES_IN_FLIGHT = 2;

	SwapChain::SwapChain()
	{
		Init();
		CR_LOG_INFO("Initialized Vulkan swap chain");
	}

	SwapChain::~SwapChain()
	{
		Destroy();
	}

	void SwapChain::Init()
	{
		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();
		VkSurfaceKHR surface = Application::GetApp().GetWindow()->GetVulkanSurface();
		SwapChainSupportDetails supportDetails = device->QuerySwapChainSupport(device->GetPhysicalDevice());

		PickDetails();

		// Create swapchain info
		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;
		createInfo.minImageCount = m_ImageCount;
		createInfo.imageFormat = m_ImageFormat.format;
		createInfo.imageColorSpace = m_ImageFormat.colorSpace;
		createInfo.presentMode = m_PresentMode;
		createInfo.imageExtent = m_Extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.preTransform = supportDetails.Capabilities.currentTransform;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		// Select exclusive vs concurrent mode
		QueueFamilyIndices indices = device->GetQueueIndices();
		uint32_t queueFamilyIndices[] = { indices.GraphicsQueue.value(), indices.PresentQueue.value() };
		if (indices.GraphicsQueue != indices.PresentQueue)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else 
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices = nullptr;
		}

		// Create swap chain
		VK_CHECK_RESULT(vkCreateSwapchainKHR(device->GetLogicalDevice(), &createInfo, nullptr, &m_SwapChain));

		// Get swap chain image handles
		VK_CHECK_RESULT(vkGetSwapchainImagesKHR(device->GetLogicalDevice(), m_SwapChain, &m_ImageCount, nullptr));
		m_Images.resize(m_ImageCount);

		std::vector<VkImage> images(m_ImageCount);
		VK_CHECK_RESULT(vkGetSwapchainImagesKHR(device->GetLogicalDevice(), m_SwapChain, &m_ImageCount, images.data()));

		for (int i = 0; i < m_ImageCount; i++)
		{
			m_Images[i].Image = images[i];
		}

		CreateImageViews();
		CreateFramebuffers();
		CreateCommandBuffers();
		CreateSynchronizationObjects();
	}

	void SwapChain::Destroy()
	{
		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();

		// Destroy framebuffers
		for (auto framebuffer : m_Framebuffers)
		{
			vkDestroyFramebuffer(device->GetLogicalDevice(), framebuffer, nullptr);
		}

		// Destroy render pass
		vkDestroyRenderPass(device->GetLogicalDevice(), m_RenderPass, nullptr);

		// Destroy image views
		for (auto image : m_Images)
		{
			vkDestroyImageView(device->GetLogicalDevice(), image.ImageView, nullptr);
		}

		// Destroy synchronization objects
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vkDestroySemaphore(device->GetLogicalDevice(), m_PresentCompleteSemaphores[i], nullptr);
			vkDestroyFence(device->GetLogicalDevice(), m_WaitFences[i], nullptr);
		}

		for (int i = 0; i < 3; i++)
		{
			vkDestroySemaphore(device->GetLogicalDevice(), m_RenderCompleteSemaphores[i], nullptr);
		}

		// Destroy command pool
		vkDestroyCommandPool(device->GetLogicalDevice(), m_CommandPool, nullptr);

		// Destroy swap chain
		vkDestroySwapchainKHR(device->GetLogicalDevice(), m_SwapChain, nullptr);
	}

	void SwapChain::BeginFrame()
	{
		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();
		VK_CHECK_RESULT(vkAcquireNextImageKHR(device->GetLogicalDevice(), m_SwapChain, UINT64_MAX, m_PresentCompleteSemaphores[m_CurrentBufferIndex], VK_NULL_HANDLE, &m_CurrentImageIndex));
	}

	void SwapChain::Present()
	{
		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();

		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &m_PresentCompleteSemaphores[m_CurrentBufferIndex];
		submitInfo.pWaitDstStageMask = &waitStage;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_CommandBuffers[m_CurrentBufferIndex];
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &m_RenderCompleteSemaphores[m_CurrentImageIndex];

		VK_CHECK_RESULT(vkResetFences(device->GetLogicalDevice(), 1, &m_WaitFences[m_CurrentBufferIndex]));
		VK_CHECK_RESULT(vkQueueSubmit(device->GetGraphicsQueue(), 1, &submitInfo, m_WaitFences[m_CurrentBufferIndex]));

		VkResult result = QueuePresent(device->GetGraphicsQueue(), m_CurrentImageIndex, m_RenderCompleteSemaphores[m_CurrentImageIndex]);

		if (result != VK_SUCCESS)
		{
			Resize();
		}

		m_CurrentBufferIndex = (m_CurrentBufferIndex + 1) % MAX_FRAMES_IN_FLIGHT;
		VK_CHECK_RESULT(vkWaitForFences(device->GetLogicalDevice(), 1, &m_WaitFences[m_CurrentBufferIndex], VK_TRUE, UINT64_MAX));
	}

	void SwapChain::PickDetails()
	{
		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();
		SwapChainSupportDetails supportDetails = device->QuerySwapChainSupport(device->GetPhysicalDevice());

		// Select format
		std::vector<VkSurfaceFormatKHR> formats = supportDetails.Formats;
		VkSurfaceFormatKHR selectedFormat = formats[0];
		for (const auto& availableFormat : formats)
		{
			// NOTE: Vulkan Tutorial recommends VK_FORMAT_B8G8R8A8_SRGB claiming to give better color accuracy but Sascha willems and ImGUI use VK_FORMAT_B8G8R8A8_UNORM
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM)
			{
				selectedFormat = availableFormat;
			}
		}
		
		// Select present mode
		std::vector<VkPresentModeKHR> presentModes = supportDetails.PresentModes;
		VkPresentModeKHR selectedPresentMode = VK_PRESENT_MODE_FIFO_KHR;
		for (const auto& availablePresentMode : presentModes)
		{
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				selectedPresentMode = availablePresentMode;
			}
		}

		// Select extent
		VkSurfaceCapabilitiesKHR capabilities = supportDetails.Capabilities;
		VkExtent2D selectedExtent;
		if (capabilities.currentExtent.width != UINT32_MAX)
		{
			selectedExtent = capabilities.currentExtent;
		}
		else
		{
			glm::ivec2 windowSizePixels = Application::GetApp().GetWindow()->GetFramebufferSize();
			VkExtent2D actualExtent =
			{
				static_cast<uint32_t>(windowSizePixels.x),
				static_cast<uint32_t>(windowSizePixels.y)
			};

			actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			selectedExtent = actualExtent;
		}

		// Select image count
		uint32_t imageCount = capabilities.minImageCount + 1;
		if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
		{
			imageCount = capabilities.maxImageCount;
		}

		m_ImageFormat = selectedFormat;
		m_PresentMode = selectedPresentMode;
		m_Extent = selectedExtent;
		m_ImageCount = imageCount;
		m_MinImageCount = capabilities.minImageCount;
	}

	void SwapChain::CreateImageViews()
	{
		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();

		// Create image views
		for (int i = 0; i < m_Images.size(); i++)
		{
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = m_Images[i].Image;

			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = m_ImageFormat.format;

			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			VK_CHECK_RESULT(vkCreateImageView(device->GetLogicalDevice(), &createInfo, nullptr, &m_Images[i].ImageView));
		}
	}

	void SwapChain::CreateFramebuffers()
	{
		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();

		// Color attachment
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = m_ImageFormat.format;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		// Reference to color attachment
		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Create subpass using color attachment reference
		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		// Create subpass for render pass that waits for the swapchain to be done reading the image
		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		// Create render pass
		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		VK_CHECK_RESULT(vkCreateRenderPass(device->GetLogicalDevice(), &renderPassInfo, nullptr, &m_RenderPass));

		// Create framebuffer for each image in the swap chain
		m_Framebuffers.resize(m_Images.size());
		for (size_t i = 0; i < m_Images.size(); i++)
		{
			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = m_RenderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = &m_Images[i].ImageView;
			framebufferInfo.width = m_Extent.width;
			framebufferInfo.height = m_Extent.height;
			framebufferInfo.layers = 1;

			VK_CHECK_RESULT(vkCreateFramebuffer(device->GetLogicalDevice(), &framebufferInfo, nullptr, &m_Framebuffers[i]));
		}
	}

	void SwapChain::CreateCommandBuffers()
	{
		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();
		QueueFamilyIndices queueIndices = device->GetQueueIndices();

		// Create command pool
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = queueIndices.GraphicsQueue.value();
		poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Optional

		VK_CHECK_RESULT(vkCreateCommandPool(device->GetLogicalDevice(), &poolInfo, nullptr, &m_CommandPool));

		m_CommandBuffers.resize(m_Framebuffers.size());

		// Create command buffers
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = m_CommandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t)m_CommandBuffers.size();

		VK_CHECK_RESULT(vkAllocateCommandBuffers(device->GetLogicalDevice(), &allocInfo, m_CommandBuffers.data()));
	}

	void SwapChain::CreateSynchronizationObjects()
	{
		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();

		m_PresentCompleteSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		m_RenderCompleteSemaphores.resize(3); // number of swapchain images
		m_WaitFences.resize(MAX_FRAMES_IN_FLIGHT);

		// Semaphore info
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		// Fence info
		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		// Create synchronization objects
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			VK_CHECK_RESULT(vkCreateSemaphore(device->GetLogicalDevice(), &semaphoreInfo, nullptr, &m_PresentCompleteSemaphores[i]));
			VK_CHECK_RESULT(vkCreateFence(device->GetLogicalDevice(), &fenceInfo, nullptr, &m_WaitFences[i]));
		}

		for (int i = 0; i < 3; i++)
		{
			VK_CHECK_RESULT(vkCreateSemaphore(device->GetLogicalDevice(), &semaphoreInfo, nullptr, &m_RenderCompleteSemaphores[i]));
		}		
	}

	void SwapChain::Resize()
	{
		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();

		vkDeviceWaitIdle(device->GetLogicalDevice());

		// Recreate swap chain
		Destroy();
		Init();
	}

	VkResult SwapChain::QueuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore)
	{
		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &waitSemaphore;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &m_SwapChain;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr;

		return vkQueuePresentKHR(queue, &presentInfo);
	}

	uint32_t SwapChain::GetFramesInFlight()
	{
		return MAX_FRAMES_IN_FLIGHT;
	}

}