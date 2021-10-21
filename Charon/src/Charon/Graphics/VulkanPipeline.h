#pragma once
#include "Charon/Graphics/Shader.h"
#include <vulkan/vulkan.h>

namespace Charon {

	class VulkanPipeline
	{
	public:
		VulkanPipeline(Ref<Shader> shader, VkRenderPass renderPass);
		~VulkanPipeline();

	public:
		inline VkPipeline GetPipeline() { return m_Pipeline; }
		inline VkPipelineLayout GetPipelineLayout() { return m_PipelineLayout; }

	private:
		void Init();

	private:
		VkPipeline m_Pipeline = nullptr;
		VkPipelineLayout m_PipelineLayout = nullptr;

		Ref<Shader> m_Shader;
		VkRenderPass m_RenderPass;
	};

}