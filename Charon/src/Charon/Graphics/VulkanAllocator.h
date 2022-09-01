#pragma once
#include "Charon/Core/Core.h"
#include "VulkanDevice.h"
#include <vk_mem_alloc.h>

namespace Charon {

	class VulkanAllocator
	{
	public:
		VulkanAllocator(const std::string& tag);
		~VulkanAllocator();
	
	public:
		VmaAllocation AllocateBuffer(const VkBufferCreateInfo& bufferCreateInfo, VmaMemoryUsage usage, VkBuffer& outBuffer);
		VmaAllocation AllocateImage(const VkImageCreateInfo& imageCreateInfo, VmaMemoryUsage usage, VkImage& outImage);

		void DestroyBuffer(VkBuffer buffer, VmaAllocation allocation);
		void DestroyImage(VkImage image, VmaAllocation allocation);
		
		template<typename T>
		T* MapMemory(VmaAllocation allocation)
		{
			T* outData;
			vmaMapMemory(GetVMAAllocator(), allocation, (void**)&outData);
			return outData;
		}

		void UnmapMemory(VmaAllocation allocation);

	public:
		static void Init(Ref<VulkanDevice> device);
		static void Shutdown();

		static VmaAllocator& GetVMAAllocator();

		static uint64_t GetVulkanDeviceAddress(VkBuffer handle);
	private:
		std::string m_Tag;
	};

}
