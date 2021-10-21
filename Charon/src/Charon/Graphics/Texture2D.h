#pragma once
#include "Charon/Graphics/Image.h"
#include "Charon/Graphics/VulkanTools.h"
#include "VulkanAllocator.h"
#include <string>

namespace Charon {

	class Texture2D
	{
	public:
		Texture2D(const std::string& path);
		~Texture2D();

		inline const VkDescriptorImageInfo& GetDescriptorImageInfo() const { return m_Image->GetDescriptorImageInfo(); }

	private:
		std::string m_Path;
		Ref<Image> m_Image;

		uint8_t* m_LocalData = nullptr;
		uint32_t m_Width = 0, m_Height = 0;
	};

}
