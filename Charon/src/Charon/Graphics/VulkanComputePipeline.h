#pragma once
#include "Charon/Graphics/Shader.h"
#include <vulkan/vulkan.h>

namespace Charon {

	class VulkanComputePipeline
	{
	public:
		VulkanComputePipeline(Ref<Shader> Shader);
		~VulkanComputePipeline();

	public:
		inline VkPipeline GetPipeline() { return m_Pipeline; }
		inline VkPipelineLayout GetPipelineLayout() { return m_PipelineLayout; }

	private:
		void Init();

	private:
		VkPipeline m_Pipeline = nullptr;
		VkPipelineLayout m_PipelineLayout = nullptr;

		Ref<Shader> m_Shader;
	};

}