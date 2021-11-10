#include "pch.h"
#include "VulkanComputePipeline.h"
#include "Charon/Core/Application.h"
#include "Charon/Graphics/VulkanTools.h"

namespace Charon {

	VulkanComputePipeline::VulkanComputePipeline(Ref<Shader> shader)
		:  m_Shader(shader)
	{
		Init();
		CR_LOG_INFO("Initialized Vulkan compute pipeline");
	}

	VulkanComputePipeline::~VulkanComputePipeline()
	{
		VkDevice device = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();

		vkDestroyPipeline(device, m_Pipeline, nullptr);
		vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
	}

	void VulkanComputePipeline::Init()
	{
		VkDevice device = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();

		const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts = m_Shader->GetDescriptorSetLayouts();

		// Set pipeline layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = descriptorSetLayouts.size();
		pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();

		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout));

		// Create compute pipeline
		VkComputePipelineCreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		computePipelineCreateInfo.layout = m_PipelineLayout;
		computePipelineCreateInfo.stage = m_Shader->GetShaderCreateInfo()[0]; // TODO: Check to make sure to get right stage for compute
		VK_CHECK_RESULT(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &m_Pipeline));

	}

}