#include "ParticleLayer.h"
#include "Charon/Core/Application.h"
#include "Charon/Graphics/Renderer.h"
#include "Charon/Graphics/SceneRenderer.h"
#include <glm/gtc/type_ptr.hpp>
#include "Charon/ImGui/imgui_impl_vulkan_with_textures.h"
#include "imgui/imgui.h"

namespace Charon {

	ParticleLayer::ParticleLayer()
		: Layer("Particle")
	{
		Init();
	}

	ParticleLayer::~ParticleLayer()
	{
	}

	void ParticleLayer::Init()
	{
		m_Camera = CreateRef<Camera>(glm::perspectiveFov(glm::radians(45.0f), 1280.0f, 720.0f, 0.1f, 100.0f));

		m_MaxQauds = 4;
		m_MaxIndices = m_MaxQauds * 6;

		// TODO: change vertex size
		m_StorageBuffer = CreateRef<StorageBuffer>(sizeof(Vertex) * 4 * m_MaxQauds);
		m_ComputeShader = CreateRef<Shader>("assets/shaders/compute.shader");

		auto renderer = Application::GetApp().GetRenderer();
		VkDevice device = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();
		
		// Set storage buffer binding
		VkDescriptorSetLayoutBinding setLayoutBinding{};
		setLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		setLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		setLayoutBinding.binding = 0;
		setLayoutBinding.descriptorCount = 1;

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
		descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutCreateInfo.pBindings = &setLayoutBinding;
		descriptorSetLayoutCreateInfo.bindingCount = 1;

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &m_ComputeDescriptorSetLayout));

		// Create PipelineLayout
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = 1;
		pipelineLayoutCreateInfo.pSetLayouts = &m_ComputeDescriptorSetLayout;

		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &m_ComputePipelineLayout));

		// Create DescriptorSet
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.pSetLayouts = &m_ComputeDescriptorSetLayout;
		descriptorSetAllocateInfo.descriptorSetCount = 1;

		m_ComputeDescriptorSet = renderer->AllocateDescriptorSet(descriptorSetAllocateInfo);

		// Write to the DescriptorSet with data from the storage buffer
		VkWriteDescriptorSet writeDescriptorSet{};
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = m_ComputeDescriptorSet;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writeDescriptorSet.dstBinding = 0;
		writeDescriptorSet.pBufferInfo = &m_StorageBuffer->getDescriptorBufferInfo();
		writeDescriptorSet.descriptorCount = 1;

		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, NULL);

		// Create ComputePipeline with layer and compute shader
		VkComputePipelineCreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		computePipelineCreateInfo.layout = m_ComputePipelineLayout;
		computePipelineCreateInfo.stage = m_ComputeShader->GetShaderCreateInfo()[0];
		VK_CHECK_RESULT(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &m_ComputePipeline));

		// Create IndexBuffer with predefined indices
		m_Indices = new uint16_t[m_MaxIndices];
		uint32_t offset = 0;
		for (uint32_t i = 0; i < m_MaxIndices; i += 6)
		{
			m_Indices[i + 0] = offset + 0;
			m_Indices[i + 1] = offset + 1;
			m_Indices[i + 2] = offset + 2;

			m_Indices[i + 3] = offset + 2;
			m_Indices[i + 4] = offset + 3;
			m_Indices[i + 5] = offset + 0;

			offset += 4;
		}

		m_IndexBuffer = CreateRef<IndexBuffer>(m_Indices, sizeof(uint16_t) * m_MaxIndices, m_MaxIndices);

		// Create shader and pipeline used to draw particles
		Ref<SwapChain> swapChain = Application::GetApp().GetVulkanSwapChain();
		
		std::vector<VkVertexInputAttributeDescription> vertexInputAttributes(2);

		// Vertex 0: Position
		vertexInputAttributes[0].binding = 0;
		vertexInputAttributes[0].location = 0;
		vertexInputAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertexInputAttributes[0].offset = 0;

		// Vertex 1: Color
		vertexInputAttributes[1].binding = 0;
		vertexInputAttributes[1].location = 1;
		vertexInputAttributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertexInputAttributes[1].offset = 12 + 4;

		uint32_t stride = sizeof(glm::vec3) + sizeof(glm::vec3) + 4 + 4;

		m_Shader = CreateRef<Shader>("assets/shaders/particle.shader");
		m_Pipeline = CreateRef<VulkanPipeline>(m_Shader, swapChain->GetRenderPass(), vertexInputAttributes, stride);
	}

	void ParticleLayer::OnUpdate()
	{
		m_Camera->Update();

		auto renderer = Application::GetApp().GetRenderer();
		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();

		// Create command buffer from device on the grapics queue and start recording
		// TODO: Move to compute queue
		VkCommandBuffer commandBuffer = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		
		// Re-Allocate DescriptorSet
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.pSetLayouts = &m_ComputeDescriptorSetLayout;
		descriptorSetAllocateInfo.descriptorSetCount = 1;

		m_ComputeDescriptorSet = renderer->AllocateDescriptorSet(descriptorSetAllocateInfo);

		// Re-Write to the DescriptorSet to update with new data from storage buffer
		VkWriteDescriptorSet writeDescriptorSet{};
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = m_ComputeDescriptorSet;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writeDescriptorSet.dstBinding = 0;
		writeDescriptorSet.pBufferInfo = &m_StorageBuffer->getDescriptorBufferInfo();
		writeDescriptorSet.descriptorCount = 1;

		vkUpdateDescriptorSets(device->GetLogicalDevice(), 1, &writeDescriptorSet, 0, NULL);

		// Bind the Compute Pipeline and the DescriptorSet and then dispach the compute shader
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ComputePipeline);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ComputePipelineLayout, 0, 1, &m_ComputeDescriptorSet, 0, 0);
		vkCmdDispatch(commandBuffer, 1, 1, 1);

		// Flush the command buffer and free it
		device->FlushCommandBuffer(commandBuffer, true);
	}

	void ParticleLayer::OnRender()
	{
		auto renderer = Application::GetApp().GetRenderer();
		VkCommandBuffer commandBuffer = renderer->GetActiveCommandBuffer();

		renderer->BeginScene(m_Camera);
		renderer->BeginRenderPass(renderer->GetFramebuffer());

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetPipeline());

		VkDeviceSize offset = 0;
		VkBuffer vertexBuffer = m_StorageBuffer->GetBuffer();
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &offset);
		vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT16);
		glm::mat4 dummyTransform = glm::mat4(1.0f);
		vkCmdPushConstants(commandBuffer, m_Pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &dummyTransform);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetPipelineLayout(), 0, renderer->GetDescriptorSets().size(), renderer->GetDescriptorSets().data(), 0, nullptr);

		vkCmdDrawIndexed(commandBuffer, m_MaxIndices, 1, 0, 0, 0);

		renderer->EndRenderPass();

		renderer->BeginRenderPass();
		renderer->RenderUI();
		renderer->EndRenderPass();

		renderer->EndScene();
	}

	void ParticleLayer::OnImGUIRender()
	{
		ImGui::Begin("Viewport");

		auto& descriptorInfo = SceneRenderer::GetFinalBuffer()->GetImage(0)->GetDescriptorImageInfo();
		ImTextureID imTex = ImGui_ImplVulkan_AddTexture(descriptorInfo.sampler, descriptorInfo.imageView, descriptorInfo.imageLayout);

		const auto& fbSpec = SceneRenderer::GetFinalBuffer()->GetSpecification();
		float width = ImGui::GetContentRegionAvail().x;
		float aspect = (float)fbSpec.Height / (float)fbSpec.Width;

		ImGui::Image(imTex, { width, width * aspect }, ImVec2(0, 1), ImVec2(1, 0));

		ImGui::End();
	}

}