#include "pch.h"
#include "VulkanComputePipeline.h"
#include "Charon/Core/Application.h"
#include "Charon/Graphics/VulkanTools.h"

namespace Charon {

	VulkanComputePipeline::VulkanComputePipeline(Ref<Shader> shader, VkPipelineLayout layout)
		:  m_Shader(shader), m_PipelineLayout(layout)
	{
		Init();
		CR_LOG_INFO("Initialized Vulkan compute pipeline");
	}

	VulkanComputePipeline::~VulkanComputePipeline()
	{
		VkDevice device = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();

		vkDestroyPipeline(device, m_Pipeline, nullptr);
		if (m_OwnLayout)
			vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
	}

	void VulkanComputePipeline::Init()
	{
		VkDevice device = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();

		const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts = m_Shader->GetDescriptorSetLayouts();

		if (!m_PipelineLayout)
		{
			// Set pipeline layout
			VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
			pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutInfo.setLayoutCount = descriptorSetLayouts.size();
			pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();

			const auto& pushConstantRanges = m_Shader->GetPushConstantRanges();
			std::vector<VkPushConstantRange> vulkanPushConstantRanges;
			vulkanPushConstantRanges.reserve(pushConstantRanges.size());
			for (const auto& pushConstangeRange : pushConstantRanges)
			{
				auto& pcr = vulkanPushConstantRanges.emplace_back();
				pcr.stageFlags = pushConstangeRange.ShaderStage;
				pcr.offset = pushConstangeRange.Offset;
				pcr.size = pushConstangeRange.Size;
			}

			pipelineLayoutInfo.pushConstantRangeCount = (uint32_t)vulkanPushConstantRanges.size();
			pipelineLayoutInfo.pPushConstantRanges = vulkanPushConstantRanges.data();

			VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout));
			m_OwnLayout = true;
		}

		// Create compute pipeline
		VkComputePipelineCreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		computePipelineCreateInfo.layout = m_PipelineLayout;
		computePipelineCreateInfo.stage = m_Shader->GetShaderCreateInfo()[0]; // TODO: Check to make sure to get right stage for compute
		VK_CHECK_RESULT(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &m_Pipeline));
	}

}