#pragma once
#include <vulkan/vulkan.h>

namespace Charon {

	class VulkanInstance
	{
	public:
		VulkanInstance(const std::string& name);
		~VulkanInstance();

	public:
		inline VkInstance GetInstanceHandle() { return m_Instance; }

	private:
		void Init();
		void CreateDebugCallback();

	private:
		VkInstance m_Instance = nullptr;
		std::string m_Name;

		PFN_vkCreateDebugUtilsMessengerEXT m_CreateDebugUtilsMessengerEXT = nullptr;
		PFN_vkDestroyDebugUtilsMessengerEXT m_DestroyDebugUtilsMessengerEXT = nullptr;
		VkDebugUtilsMessengerEXT m_DebugUtilsMessenger = nullptr;
	};

}