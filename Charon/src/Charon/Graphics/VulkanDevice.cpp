#include "pch.h"
#include "VulkanDevice.h"
#include "VulkanInstance.h"
#include "Charon/Core/Application.h"
#include "Charon/Graphics/VulkanTools.h"
#include "VulkanExtensions.h"

static std::vector<const char*> s_DeviceExtensions =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME
};

namespace Charon {

	VulkanDevice::VulkanDevice()
	{
		Init();
		CR_LOG_INFO("Initialized Vulkan device");
	}

	VulkanDevice::~VulkanDevice()
	{
		vkDestroyCommandPool(m_LogicalDevice, m_CommandPool, nullptr);
		vkDestroyDevice(m_LogicalDevice, nullptr);
	}

	void VulkanDevice::Init()
	{
		VkInstance instance = Application::GetApp().GetVulkanInstance()->GetInstanceHandle();

		// Get device info
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
		CR_ASSERT(deviceCount, "Could not find any devices that support Vulkan");

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		// Loop though devices to find a suitable one
		QueueFamilyIndices indices;
		for (int i = 0; i < deviceCount; i++)
		{
			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(devices[i], &deviceProperties);

			indices = FindQueueIndices(devices[i]);

			if (IsDeviceSuitable(devices[i]))
			{
				m_SwapChainSupportDetails = QuerySwapChainSupport(devices[i]);
				m_QueueIndices = indices;
				m_PhysicalDevice = devices[i];
				CR_LOG_INFO("Selected GPU: {0}", deviceProperties.deviceName);
			}
		}

		CR_ASSERT(m_PhysicalDevice != VK_NULL_HANDLE, "Could not find any suitable device");

		VkPhysicalDeviceFeatures f;
		vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &f);

		// Create info for all queues
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { indices.GraphicsQueue.value(), indices.PresentQueue.value(), indices.TransferQueue.value() };

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceVulkan12Features v12Features{};
		v12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		v12Features.shaderBufferInt64Atomics = true;
		v12Features.bufferDeviceAddress = true;
		v12Features.descriptorBindingPartiallyBound = true;
		v12Features.descriptorIndexing = true;
		v12Features.runtimeDescriptorArray = true;
		v12Features.bufferDeviceAddress = true;

#define RTX 1
#if RTX
		// Ray Tracing
		VkPhysicalDeviceRobustness2FeaturesEXT robustness2Features{};
		robustness2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
		robustness2Features.nullDescriptor = VK_TRUE;

		/*VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{};
		bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
		bufferDeviceAddressFeatures.pNext = &robustness2Features;
		bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;*/

		VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};
		accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
		accelerationStructureFeatures.pNext = &robustness2Features;
		accelerationStructureFeatures.accelerationStructure = VK_TRUE;
		accelerationStructureFeatures.accelerationStructureCaptureReplay = VK_FALSE;
		accelerationStructureFeatures.accelerationStructureIndirectBuild = VK_FALSE;
		accelerationStructureFeatures.accelerationStructureHostCommands = VK_FALSE;
		accelerationStructureFeatures.descriptorBindingAccelerationStructureUpdateAfterBind = VK_FALSE;

		VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{};
		rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
		rayTracingPipelineFeatures.pNext = &accelerationStructureFeatures;
		rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
		rayTracingPipelineFeatures.rayTracingPipelineShaderGroupHandleCaptureReplay = VK_FALSE;
		rayTracingPipelineFeatures.rayTracingPipelineShaderGroupHandleCaptureReplayMixed = VK_FALSE;
		rayTracingPipelineFeatures.rayTracingPipelineTraceRaysIndirect = VK_TRUE;
		rayTracingPipelineFeatures.rayTraversalPrimitiveCulling = VK_TRUE;

		/*VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures{};
		descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
		descriptorIndexingFeatures.pNext = &rayTracingPipelineFeatures;
		descriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
		descriptorIndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;*/

		v12Features.pNext = &rayTracingPipelineFeatures;

		s_DeviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
		s_DeviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
		s_DeviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
		s_DeviceExtensions.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
		s_DeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
		s_DeviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
		s_DeviceExtensions.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
		s_DeviceExtensions.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
#endif

		// Required device features
		VkPhysicalDeviceFeatures deviceFeatures{};

		// Logical device info
		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pNext = &v12Features;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(s_DeviceExtensions.size());
		createInfo.ppEnabledExtensionNames = s_DeviceExtensions.data();
		createInfo.pEnabledFeatures = &deviceFeatures;

		// Create logical device
		VK_CHECK_RESULT(vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_LogicalDevice));

		LoadDeviceExtensions(m_LogicalDevice);

		// Create queue handles
		vkGetDeviceQueue(m_LogicalDevice, indices.GraphicsQueue.value(), 0, &m_GraphicsQueue);
		vkGetDeviceQueue(m_LogicalDevice, indices.PresentQueue.value(), 0, &m_PresentQueue);

		// Create command pool
		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = indices.GraphicsQueue.value();
		VK_CHECK_RESULT(vkCreateCommandPool(m_LogicalDevice, &poolInfo, nullptr, &m_CommandPool));
	}

	VkCommandBuffer VulkanDevice::CreateCommandBuffer(VkCommandBufferLevel level, bool begin)
	{
		// Create command buffer
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = m_CommandPool;
		allocInfo.level = level;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		VK_CHECK_RESULT(vkAllocateCommandBuffers(m_LogicalDevice, &allocInfo, &commandBuffer));

		// Begin command buffer if specified
		if (begin)
		{
			VkCommandBufferBeginInfo commandBufferBeginInfo{};
			commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));
		}

		return commandBuffer;
	}

	void VulkanDevice::FlushCommandBuffer(VkCommandBuffer commandBuffer, bool free)
	{
		CR_ASSERT(commandBuffer != VK_NULL_HANDLE, "Command buffer is invalid");

		// End command buffers
		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));
	
		// Submit info
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		// Create fence
		VkFenceCreateInfo fenceCreateInfo{};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

		VkFence fence;
		VK_CHECK_RESULT(vkCreateFence(m_LogicalDevice, &fenceCreateInfo, nullptr, &fence));

		// Submit command buffer and signal fence when it's done
		VK_CHECK_RESULT(vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, fence));
		VK_CHECK_RESULT(vkWaitForFences(m_LogicalDevice, 1, &fence, VK_TRUE, UINT32_MAX));

		// Destroy fence
		vkDestroyFence(m_LogicalDevice, fence, nullptr);
		
		// Free command buffer if specified
		if (free)
		{
			vkFreeCommandBuffers(m_LogicalDevice, m_CommandPool, 1, &commandBuffer);
		}
	}

	bool VulkanDevice::IsDeviceSuitable(VkPhysicalDevice device)
	{
		m_AccelerationStructureProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
		m_RayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
		m_RayTracingPipelineProperties.pNext = &m_AccelerationStructureProperties;

		m_PhysicalDeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		m_PhysicalDeviceProperties.pNext = &m_RayTracingPipelineProperties;

		vkGetPhysicalDeviceProperties2(device, &m_PhysicalDeviceProperties);

		QueueFamilyIndices indices = FindQueueIndices(device);
			
		// Check is all required extensions are supported
		bool extensionsSupported = CheckDeviceExtensionSupport(device);
		bool swapChainAdequate = false;
		if (extensionsSupported)
		{
			// Check to make sure there supported formats and present modes
			SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.Formats.empty() && !swapChainSupport.PresentModes.empty();
		}

		// Check to make sure the device is dedicated and has all required queues
		return m_PhysicalDeviceProperties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && indices.isComplete() && extensionsSupported && swapChainAdequate;
	}

	bool VulkanDevice::CheckDeviceExtensionSupport(VkPhysicalDevice device)
	{
		// Get extension info
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		// Convert s_DeviceExtensions to string set
		std::set<std::string> requiredExtensions(s_DeviceExtensions.begin(), s_DeviceExtensions.end());

		// Remove one extension from requiredExtensions for every one found
		for (const auto& extension : availableExtensions) {
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

	QueueFamilyIndices VulkanDevice::FindQueueIndices(VkPhysicalDevice device)
	{
		// Get queue family info
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		QueueFamilyIndices indices;
		for (int i = 0; i < queueFamilyCount; i++)
		{
			// Find graphics queue
			if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.GraphicsQueue = i;
			}

			// Find present queue
			// TODO: Pick best queue for presenting
			VkSurfaceKHR surface = Application::GetApp().GetWindow()->GetVulkanSurface();
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

			if (presentSupport)
			{
				indices.PresentQueue = i;
			}

			// Find transfer queue
			if ((queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) && ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
			{
				indices.TransferQueue = i;
			}
		}

		return indices;
	}

	SwapChainSupportDetails VulkanDevice::QuerySwapChainSupport(VkPhysicalDevice device)
	{
		SwapChainSupportDetails details;

		// Query surface capabilities
		VkSurfaceKHR surface = Application::GetApp().GetWindow()->GetVulkanSurface();
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.Capabilities);

		// Query formats
		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

		if (formatCount != 0) {
			details.Formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.Formats.data());
		}

		// Query present modes
		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

		if (presentModeCount != 0) {
			details.PresentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.PresentModes.data());
		}

		return details;
	}

}