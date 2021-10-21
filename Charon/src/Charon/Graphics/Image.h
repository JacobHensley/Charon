#pragma once
#include "Charon/Graphics/VulkanTools.h"
#include "VulkanAllocator.h"

namespace Charon {

	struct ImageInfo
	{
		VkImage Image = nullptr;
		VkImageView ImageView = nullptr;
		VkSampler Sampler = nullptr;
		VmaAllocation MemoryAlloc = nullptr;
	};

	struct ImageSpecification
	{
		uint8_t* Data;
		uint32_t Width;
		uint32_t Height;
		VkFormat Format;
		uint32_t LayerCount = 1;
		VkImageUsageFlags Usage;
		VkSampleCountFlagBits SampleCount = VK_SAMPLE_COUNT_1_BIT;
		bool UseStagingBuffer = true;
	};

	class Image
	{
	public:
		Image(ImageSpecification specification);
		~Image();

	public:
		inline const ImageSpecification& GetSpecification() const { return m_Specification; }
		inline const VkDescriptorImageInfo& GetDescriptorImageInfo() const { return m_DescriptorImageInfo; }

	public:
		static bool IsDepthFormat(VkFormat format);
		static bool IsStencilFormat(VkFormat format);

	private:
		void Init();

	private:
		ImageInfo m_ImageInfo;
		VkDescriptorImageInfo m_DescriptorImageInfo;
		uint32_t m_Size = 0;

		ImageSpecification m_Specification;
	};

}