#pragma once
#include "Charon/Graphics/Shader.h"
#include "Charon/Graphics/VertexBufferLayout.h"
#include <vulkan/vulkan.h>

namespace Charon {

	struct PipelineSpecification
	{
		Ref<Shader> Shader = nullptr;
		VertexBufferLayout* Layout = nullptr;
		VkRenderPass TargetRenderPass = nullptr;
		bool WriteDepth = true;
	};

	class VulkanPipeline
	{
	public:
		VulkanPipeline(const PipelineSpecification& specification);
		~VulkanPipeline();

	public:
		inline VkPipeline GetPipeline() { return m_Pipeline; }
		inline VkPipelineLayout GetPipelineLayout() { return m_PipelineLayout; }

	private:
		void Init();

	private:
		PipelineSpecification m_Specification;

		VkPipeline m_Pipeline = nullptr;
		VkPipelineLayout m_PipelineLayout = nullptr;
	};

}