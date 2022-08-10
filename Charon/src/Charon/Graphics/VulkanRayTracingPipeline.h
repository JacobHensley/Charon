#pragma once
#include "Charon/Graphics/Shader.h"
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
		VulkanRayTracingPipeline(const RayTracingPipelineSpecification& specification);
		~VulkanRayTracingPipeline();

	public:
		inline VkPipeline GetPipeline() { return m_Pipeline; }
		inline VkPipelineLayout GetPipelineLayout() { return m_PipelineLayout; }

	private:
		void Init();

	private:
		RayTracingPipelineSpecification m_Specification;

		VkPipeline m_Pipeline = nullptr;
		VkPipelineLayout m_PipelineLayout = nullptr;

		VkDescriptorSetLayout m_DescriptorSetLayout = nullptr;

		bool m_OwnLayout = false;
		Ref<Shader> m_Shader;
	};

}