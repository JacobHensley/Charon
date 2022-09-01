#include "pch.h"
#define VMA_IMPLEMENTATION
#include "VulkanAllocator.h"
#include "Charon/Core/Application.h"
#include "Charon/Core/Log.h"

namespace Charon {

	struct VulkanAllocatorData
	{
		VmaAllocator Allocator;
	};

	static VulkanAllocatorData* s_Data = nullptr;

	VulkanAllocator::VulkanAllocator(const std::string& tag)
		: m_Tag(tag)
	{
	}

	VulkanAllocator::~VulkanAllocator()
	{
	}

	VmaAllocation VulkanAllocator::AllocateBuffer(const VkBufferCreateInfo& bufferCreateInfo, VmaMemoryUsage usage, VkBuffer& outBuffer)
	{
		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = usage;

		VmaAllocation allocation;
		VkResult result = vmaCreateBuffer(s_Data->Allocator, &bufferCreateInfo, &allocCreateInfo, &outBuffer, &allocation, nullptr);

		// Metrics
		VmaAllocationInfo allocInfo;
		vmaGetAllocationInfo(s_Data->Allocator, allocation, &allocInfo);
		CR_LOG_INFO("[{0}] - allocating buffer; size = {1}", m_Tag, allocInfo.size);

		return allocation;
	}

	VmaAllocation VulkanAllocator::AllocateImage(const VkImageCreateInfo& imageCreateInfo, VmaMemoryUsage usage, VkImage& outImage)
	{
		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = usage;

		VmaAllocation allocation;
		vmaCreateImage(s_Data->Allocator, &imageCreateInfo, &allocCreateInfo, &outImage, &allocation, nullptr);

		// Metrics
		VmaAllocationInfo allocInfo;
		vmaGetAllocationInfo(s_Data->Allocator, allocation, &allocInfo);
		CR_LOG_INFO("[{0}] - allocating image; size = {1}", m_Tag, allocInfo.size);

		return allocation;
	}

	void VulkanAllocator::DestroyBuffer(VkBuffer buffer, VmaAllocation allocation)
	{
		vmaDestroyBuffer(s_Data->Allocator, buffer, allocation);
	}

	void VulkanAllocator::DestroyImage(VkImage image, VmaAllocation allocation)
	{
		vmaDestroyImage(s_Data->Allocator, image, allocation);
	}

	void VulkanAllocator::UnmapMemory(VmaAllocation allocation)
	{
		vmaUnmapMemory(s_Data->Allocator, allocation);
	}

	void VulkanAllocator::Init(Ref<VulkanDevice> device)
	{
		s_Data = new VulkanAllocatorData();

		// Init VMA
		VmaAllocatorCreateInfo allocatorInfo = {};
		allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
		allocatorInfo.physicalDevice = device->GetPhysicalDevice();
		allocatorInfo.device = device->GetLogicalDevice();
		allocatorInfo.instance = Application::GetApp().GetVulkanInstance()->GetInstanceHandle();
		allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

		vmaCreateAllocator(&allocatorInfo, &s_Data->Allocator);

		CR_LOG_INFO("Initialized VMA");
	}

	void VulkanAllocator::Shutdown()
	{
		vmaDestroyAllocator(s_Data->Allocator);

		delete s_Data;
		s_Data = nullptr;
	}

	VmaAllocator& VulkanAllocator::GetVMAAllocator()
	{
		return s_Data->Allocator;
	}

	uint64_t VulkanAllocator::GetVulkanDeviceAddress(VkBuffer handle)
	{
		VkDevice device = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();
		VkBufferDeviceAddressInfoKHR buffer_device_address_info{};
		buffer_device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		buffer_device_address_info.buffer = handle;
		return vkGetBufferDeviceAddressKHR(device, &buffer_device_address_info);
	}
}