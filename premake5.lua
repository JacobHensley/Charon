VK_SDK_PATH = os.getenv("VK_SDK_PATH")

workspace "Charon"
	architecture "x64"
	startproject "Styx"

	configurations
	{
		"Debug",
		"Release"
	}

	flags
	{
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

IncludeDir = {}
IncludeDir["VulkanSDK"]         = VK_SDK_PATH .. "/include"
IncludeDir["GLFW"]              = "Charon/vendor/GLFW/include"
IncludeDir["glm"]               = "Charon/vendor/glm"
IncludeDir["spdlog"]            = "Charon/vendor/spdlog/include"
IncludeDir["VMA"]               = "Charon/vendor/VMA/include"
IncludeDir["SPIRVCross"]        = "Charon/vendor/SPIRV-Cross"
IncludeDir["imgui"]             = "Charon/vendor/imgui"
IncludeDir["stb_image"]         = "Charon/vendor/stb"

include "Charon/vendor/GLFW"
include "Charon/vendor/SPIRV-Cross"
include "Charon/vendor/imgui"

project "Charon"
	location "Charon"
	kind "StaticLib"
	language "C++"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin/intermediates/" .. outputdir .. "/%{prj.name}")	

	pchheader "pch.h"
	pchsource "Charon/src/pch.cpp"

	files
	{
		-- Charon
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/src/**.h",
		-- STB
		"%{prj.name}/vendor/stb/**.cpp",
		"%{prj.name}/vendor/stb/**.h",
		-- TinyGltf
		"%{prj.name}/vendor/tinygltf/**.cpp",
		"%{prj.name}/vendor/tinygltf/**.hpp",
		"%{prj.name}/vendor/tinygltf/**.h",
	}

	includedirs
	{
		"%{prj.name}/src",
		"%{prj.name}/vendor",
		"%{IncludeDir.VulkanSDK}",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.spdlog}",
		"%{IncludeDir.VMA}",
		"%{IncludeDir.SPIRVCross}",
		"%{IncludeDir.imgui}",
		"%{IncludeDir.stb_image}",
	}

	filter "system:windows"
		cppdialect "C++17"
		systemversion "latest"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "On"

	filter "configurations:Release"
		runtime "Release"
		optimize "On"

project "Styx"
	location "Styx"
	kind "ConsoleApp"
	language "C++"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin/intermediates/" .. outputdir .. "/%{prj.name}")

	files
	{
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/src/**.h",
	}

	includedirs
	{
		"%{prj.name}/src",
		"%{prj.name}/vendor",
		"Charon/src",
		"Charon/vendor",
		"%{IncludeDir.VulkanSDK}",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.spdlog}",
		"%{IncludeDir.VMA}",
		"%{IncludeDir.SPIRVCross}",
		"%{IncludeDir.imgui}",
		"%{IncludeDir.stb_image}",
	}

	links 
	{ 
		"Charon",
		"GLFW",
		"SPIRV-Cross",
		"imgui",
		VK_SDK_PATH .. "/Lib/vulkan-1.lib",
		VK_SDK_PATH .. "/Lib/shaderc_shared.lib",
	}

	filter "system:windows"
		cppdialect "C++17"
		systemversion "latest"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "On"
		
		defines 
		{
			"CR_ENABLE_ASSERTS"
		}

	filter "configurations:Release"
		runtime "Release"
		optimize "On"