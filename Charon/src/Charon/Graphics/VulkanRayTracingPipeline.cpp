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

		inline uint32_t AlignedSize(uint32_t value, uint32_t alignment)
		{
			return (value + alignment - 1) & ~(alignment - 1);
		}

		static VkStridedDeviceAddressRegionKHR GetSbtEntryStridedDeviceAddressRegion(VkBuffer buffer, uint32_t handleCount = 1)
		{
			const auto& props = Application::GetApp().GetVulkanDevice()->GetRayTracingPipelineProperties();

			uint32_t handleSizeAligned = AlignedSize(props.shaderGroupHandleSize, props.shaderGroupHandleAlignment);
			VkStridedDeviceAddressRegionKHR stridedDeviceAddressRegionKHR{};
			stridedDeviceAddressRegionKHR.deviceAddress = VulkanAllocator::GetVulkanDeviceAddress(buffer);
			stridedDeviceAddressRegionKHR.stride = handleSizeAligned;
			stridedDeviceAddressRegionKHR.size = handleCount * handleSizeAligned;
			return stridedDeviceAddressRegionKHR;
		}

		static VulkanRayTracingPipeline::RTBufferInfo CreateShaderBindingTable()
		{
			const auto& props = Application::GetApp().GetVulkanDevice()->GetRayTracingPipelineProperties();
			VulkanAllocator allocator("RayTracingPipeline");

			VulkanRayTracingPipeline::RTBufferInfo bufferInfo;

			VkBufferCreateInfo bufferCreateInfo{};
			bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferCreateInfo.size = props.shaderGroupHandleSize;
			bufferCreateInfo.usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
			bufferInfo.Memory = allocator.AllocateBuffer(bufferCreateInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, bufferInfo.Buffer);

			bufferInfo.StridedDeviceAddressRegion = GetSbtEntryStridedDeviceAddressRegion(bufferInfo.Buffer);

			return bufferInfo;
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
			Utils::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0),
			Utils::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 1),
			Utils::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 2),
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

			m_ShaderBindingTable.emplace_back(Utils::CreateShaderBindingTable());
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

			m_ShaderBindingTable.emplace_back(Utils::CreateShaderBindingTable());
		}

		if (m_Specification.ClosestHitShader)
		{
			const VkPipelineShaderStageCreateInfo& shaderStage = m_Specification.ClosestHitShader->GetShaderCreateInfo()[0];
			shaderStages.emplace_back(shaderStage);

			VkRayTracingShaderGroupCreateInfoKHR& shaderGroup = shaderGroups.emplace_back();
			shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
			shaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.closestHitShader = (uint32_t)(shaderStages.size()) - 1;
			shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;

			m_ShaderBindingTable.emplace_back(Utils::CreateShaderBindingTable());
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

		{
			const auto& props = Application::GetApp().GetVulkanDevice()->GetRayTracingPipelineProperties();

			const uint32_t handleSize = props.shaderGroupHandleSize;
			const uint32_t handleSizeAligned = Utils::AlignedSize(props.shaderGroupHandleSize, props.shaderGroupHandleAlignment);
			const uint32_t groupCount = (uint32_t)shaderGroups.size();
			const uint32_t sbtSize = groupCount * handleSizeAligned;

			m_ShaderHandleStorage.resize(sbtSize);
			VK_CHECK_RESULT(vkGetRayTracingShaderGroupHandlesKHR(device, m_Pipeline, 0, groupCount, sbtSize, m_ShaderHandleStorage.data()));

			VulkanAllocator allocator("RayTracingPipeline");
			{
				uint8_t* raygenHandle = allocator.MapMemory<uint8_t>(m_ShaderBindingTable[0].Memory);
				memcpy(raygenHandle, m_ShaderHandleStorage.data(), handleSize);
				allocator.UnmapMemory(m_ShaderBindingTable[0].Memory);
			}
			{
				uint8_t* missHandle = allocator.MapMemory<uint8_t>(m_ShaderBindingTable[1].Memory);
				memcpy(missHandle, m_ShaderHandleStorage.data() + handleSizeAligned, handleSize);
				allocator.UnmapMemory(m_ShaderBindingTable[1].Memory);
			}
			{
				uint8_t* closestHitHandle = allocator.MapMemory<uint8_t>(m_ShaderBindingTable[2].Memory);
				memcpy(closestHitHandle, m_ShaderHandleStorage.data() + handleSizeAligned * 2, handleSize);
				allocator.UnmapMemory(m_ShaderBindingTable[2].Memory);
			}
		}
	}

	

}