#include "pch.h"
#include "VulkanRayTracingPipeline.h"
#include "Charon/Core/Application.h"
#include "Charon/Graphics/VulkanTools.h"

namespace Charon {

	namespace Utils {

		static VkDescriptorSetLayoutBinding DescriptorSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding, uint32_t descriptorCount = 1)
		{
			VkDescriptorSetLayoutBinding setLayoutBinding{};
			setLayoutBinding.descriptorType = type;
			setLayoutBinding.stageFlags = stageFlags;
			setLayoutBinding.binding = binding;
			setLayoutBinding.descriptorCount = descriptorCount;
			return setLayoutBinding;
		}
	}


	VulkanRayTracingPipeline::VulkanRayTracingPipeline(const RayTracingPipelineSpecification& specification)
		:  m_Specification(specification)
	{
		Init();
		CR_LOG_INFO("Initialized Vulkan Ray Tracing Pipeline");
	}

	VulkanRayTracingPipeline::~VulkanRayTracingPipeline()
	{
		VkDevice device = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();

		vkDestroyPipeline(device, m_Pipeline, nullptr);
		if (m_OwnLayout)
			vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
	}

	void VulkanRayTracingPipeline::Init()
	{
		VkDevice device = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();

		std::vector<VkDescriptorSetLayoutBinding> layoutBindings = {
			Utils::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0)
		};

		// TODO: flags (for bindless)
		
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
		descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutCreateInfo.pBindings = layoutBindings.data();
		descriptorSetLayoutCreateInfo.bindingCount = (uint32_t)layoutBindings.size();
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &m_DescriptorSetLayout));

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = 1;
		pipelineLayoutCreateInfo.pSetLayouts = &m_DescriptorSetLayout;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &m_PipelineLayout));

		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
		std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;

		if (m_Specification.RayGenShader)
		{
			const VkPipelineShaderStageCreateInfo& shaderStage = m_Specification.RayGenShader->GetShaderCreateInfo()[0];
			shaderStages.emplace_back(shaderStage);

			VkRayTracingShaderGroupCreateInfoKHR& shaderGroup = shaderGroups.emplace_back();
			shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
			shaderGroup.generalShader = (uint32_t)(shaderStages.size()) - 1;
			shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
		}

		if (m_Specification.MissShader)
		{
			const VkPipelineShaderStageCreateInfo& shaderStage = m_Specification.MissShader->GetShaderCreateInfo()[0];
			shaderStages.emplace_back(shaderStage);

			VkRayTracingShaderGroupCreateInfoKHR& shaderGroup = shaderGroups.emplace_back();
			shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
			shaderGroup.generalShader = (uint32_t)(shaderStages.size()) - 1;
			shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
		}

		if (m_Specification.ClosestHitShader)
		{
			const VkPipelineShaderStageCreateInfo& shaderStage = m_Specification.ClosestHitShader->GetShaderCreateInfo()[0];
			shaderStages.emplace_back(shaderStage);

			VkRayTracingShaderGroupCreateInfoKHR& shaderGroup = shaderGroups.emplace_back();
			shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
			shaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.closestHitShader = (uint32_t)(shaderStages.size()) - 1;
			shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
		}

		VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCreateInfo{};
		rayTracingPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
		rayTracingPipelineCreateInfo.stageCount = (uint32_t)shaderStages.size();
		rayTracingPipelineCreateInfo.pStages = shaderStages.data();
		rayTracingPipelineCreateInfo.groupCount = (uint32_t)shaderGroups.size();
		rayTracingPipelineCreateInfo.pGroups = shaderGroups.data();
		rayTracingPipelineCreateInfo.maxPipelineRayRecursionDepth = 4;
		rayTracingPipelineCreateInfo.layout = m_PipelineLayout;
		VK_CHECK_RESULT(vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineCreateInfo, nullptr, &m_Pipeline));

		// Shader handles + binding table
	}

	

}