#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>

namespace Charon {

	struct QueueFamilyIndices
	{
		std::optional<uint32_t> GraphicsQueue;
		std::optional<uint32_t> PresentQueue;
		std::optional<uint32_t> TransferQueue;

		bool isComplete()
		{
			return GraphicsQueue.has_value() && PresentQueue.has_value() && TransferQueue.has_value();
		}
	};

	struct SwapChainSupportDetails 
	{
		VkSurfaceCapabilitiesKHR Capabilities;
		std::vector<VkSurfaceFormatKHR> Formats;
		std::vector<VkPresentModeKHR> PresentModes;
	};

	class VulkanDevice
	{
	public:
		VulkanDevice();
		~VulkanDevice();
	public:
		inline VkPhysicalDevice GetPhysicalDevice() { return m_PhysicalDevice; };
		inline VkDevice GetLogicalDevice() { return m_LogicalDevice; };

		VkCommandBuffer CreateCommandBuffer(VkCommandBufferLevel level, bool begin);
		void FlushCommandBuffer(VkCommandBuffer commandBuffer, bool free);

		inline QueueFamilyIndices GetQueueIndices() { return m_QueueIndices; }
		inline VkQueue GetGraphicsQueue() { return m_GraphicsQueue; }
		inline VkQueue GetPresentsQueue() { return m_PresentQueue; }

		inline SwapChainSupportDetails GetSwapChainSupportDetails() { return m_SwapChainSupportDetails; }
		SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);

		const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& GetRayTracingPipelineProperties() const { return m_RayTracingPipelineProperties; }
	private:
		void Init();

		bool IsDeviceSuitable(VkPhysicalDevice device);
		bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
		QueueFamilyIndices FindQueueIndices(VkPhysicalDevice device);
	private:
		VkPhysicalDevice m_PhysicalDevice = nullptr;
		VkDevice m_LogicalDevice = nullptr;

		VkCommandPool m_CommandPool = nullptr;

		VkQueue m_GraphicsQueue = nullptr;
		VkQueue m_PresentQueue = nullptr;
		VkPhysicalDeviceProperties2 m_PhysicalDeviceProperties;

		SwapChainSupportDetails m_SwapChainSupportDetails;
		QueueFamilyIndices m_QueueIndices;

		// Ray Tracing
		VkPhysicalDeviceAccelerationStructurePropertiesKHR m_AccelerationStructureProperties{};
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_RayTracingPipelineProperties{};
	};

}