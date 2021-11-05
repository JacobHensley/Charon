#include "pch.h"
#include "VulkanInstance.h"
#include "Charon/Graphics/VulkanTools.h"
#include <GLFW/glfw3.h>

static const bool s_EnableValidationLayers = true;
static const std::vector<const char*> s_ValidationLayers =
{
    "VK_LAYER_KHRONOS_validation"
};

static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        CR_LOG_ERROR("Validation layer: {0}", pCallbackData->pMessage);
        __debugbreak();
    }

    return VK_FALSE;
}

namespace Charon {

    namespace Utils {

        static bool CheckValidationSupport()
        {
            // Get layer info
            uint32_t layerCount;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
            std::vector<VkLayerProperties> availableLayers(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

            // Check to ensure all validation layers are in availableLayers
            for (const char* layerName : s_ValidationLayers)
            {
                bool found = false;

                for (const auto& layerProperties : availableLayers)
                {
                    if (strcmp(layerName, layerProperties.layerName) == 0)
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    return false;
                }
            }

            return true;
        }

        static std::vector<const char*> GetRequiredExtensions()
        {
            // GLFW window extensions
            uint32_t glfwExtensionCount = 0;
            const char** glfwExtensions;
            glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

            std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

            if (s_EnableValidationLayers) {
                extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }

            return extensions;
        }

        static void PrintAvailableExtensions()
        {
            // Get extension info
            uint32_t extensionCount = 0;
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
            std::vector<VkExtensionProperties> extensions(extensionCount);
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

            // Print available extensions
            CR_LOG_INFO("Available Vulkan extensions: {0}", extensionCount);
            for (const auto& extension : extensions)
            {
                CR_LOG_INFO("    {0}", extension.extensionName);
            }
        }

        static void PrintAvailableLayers()
        {
            // Get layer info
            uint32_t layerCount;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
            std::vector<VkLayerProperties> availableLayers(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

            // Print available layers
            CR_LOG_INFO("Available Vulkan layers: {0}", layerCount);
            for (const auto& layer : availableLayers)
            {
                CR_LOG_INFO("    {0}", layer.layerName);
            }
        }

        static void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
        {
            createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            createInfo.pfnUserCallback = VulkanDebugCallback;
        }
    }

    VulkanInstance::VulkanInstance(const std::string& name)
        : m_Name(name)
    {
	    Init();
        CR_LOG_INFO("Initialized Vulkan instance");
    }

    VulkanInstance::~VulkanInstance()
    {
        if (m_DebugUtilsMessenger != VK_NULL_HANDLE)
        {
            m_DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugUtilsMessenger, nullptr);
        }

        vkDestroyInstance(m_Instance, nullptr);
    }

    void VulkanInstance::Init()
    {
        CR_ASSERT(!(s_EnableValidationLayers && !Utils::CheckValidationSupport()), "Requested validation layers not available");

        // Application
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = m_Name.c_str();
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_2;

        std::vector<const char*> requiredExtensions = Utils::GetRequiredExtensions();

        // Instance
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();

        if (s_EnableValidationLayers)
        {
            // Set layers to use validation layers
            createInfo.enabledLayerCount = static_cast<uint32_t>(s_ValidationLayers.size());
            createInfo.ppEnabledLayerNames = s_ValidationLayers.data();

            // Create debug callback info for instaance creation
            VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
            Utils::PopulateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        // Create instance
        VK_CHECK_RESULT(vkCreateInstance(&createInfo, nullptr, &m_Instance));

        CreateDebugCallback();

        Utils::PrintAvailableExtensions();
        Utils::PrintAvailableLayers();
    }

    void VulkanInstance::CreateDebugCallback()
    {
        // Load debug utils extension functions
        m_CreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_Instance, "vkCreateDebugUtilsMessengerEXT"));
        m_DestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_Instance, "vkDestroyDebugUtilsMessengerEXT"));

        // Create debug callback and set it to m_DebugUtilsMessenger
        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        Utils::PopulateDebugMessengerCreateInfo(createInfo);
        VK_CHECK_RESULT(m_CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugUtilsMessenger));
    }

}