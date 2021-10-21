#pragma once
#include <Vulkan/vulkan.h>

namespace Charon {

	class ImGuiLayer
	{
	public: 
		ImGuiLayer();
		~ImGuiLayer();

		void BeginFrame();
		void EndFrame();

	private:
		void Init();

	private:
		VkDescriptorPool m_DescriptorPool;
	};

}