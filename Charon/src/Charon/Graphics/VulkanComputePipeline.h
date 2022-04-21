#pragma once
#include "Charon/Graphics/Shader.h"
#include <vulkan/vulkan.h>

namespace Charon {

	class VulkanComputePipeline
	{
	public:
		VulkanComputePipeline(Ref<Shader> Shader, VkPipelineLayout layout = nullptr);
		~VulkanComputePipeline();

	public:
		inline VkPipeline GetPipeline() { return m_Pipeline; }
		inline VkPipelineLayout GetPipelineLayout() { return m_PipelineLayout; }

	private:
		void Init();

	private:
		VkPipeline m_Pipeline = nullptr;
		VkPipelineLayout m_PipelineLayout = nullptr;

		bool m_OwnLayout = false;
		Ref<Shader> m_Shader;
	};

}