#pragma once
#include "Charon/Graphics/Shader.h"
#include "Charon/Graphics/VulkanAllocator.h"
#include <vulkan/vulkan.h>

namespace Charon {

	struct RayTracingPipelineSpecification
	{
		Ref<Shader> RayGenShader;
		Ref<Shader> MissShader;
		Ref<Shader> ClosestHitShader;
	};

	class VulkanRayTracingPipeline
	{
	public:
		struct RTBufferInfo
		{
			VkBuffer Buffer;
			VmaAllocation Memory;
			VkStridedDeviceAddressRegionKHR StridedDeviceAddressRegion;
		};
	public:
		VulkanRayTracingPipeline(const RayTracingPipelineSpecification& specification);
		~VulkanRayTracingPipeline();

	public:
		inline VkPipeline GetPipeline() { return m_Pipeline; }
		inline VkPipelineLayout GetPipelineLayout() { return m_PipelineLayout; }
		inline const VkDescriptorSetLayout& GetDescriptorSetLayout() { return m_DescriptorSetLayout; }
		const std::vector<RTBufferInfo>& GetShaderBindingTable() { return m_ShaderBindingTable; }
	private:
		void Init();
	private:
		RayTracingPipelineSpecification m_Specification;

		VkPipeline m_Pipeline = nullptr;
		VkPipelineLayout m_PipelineLayout = nullptr;
		VkDescriptorSetLayout m_DescriptorSetLayout = nullptr;

		std::vector<uint8_t> m_ShaderHandleStorage;
		std::vector<RTBufferInfo> m_ShaderBindingTable;

		bool m_OwnLayout = false;
		Ref<Shader> m_Shader;
	};

}