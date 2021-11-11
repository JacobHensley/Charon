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

		m_MaxQauds = 1025;
		m_MaxIndices = m_MaxQauds * 6;

		m_ParticleBuffer = CreateRef<StorageBuffer>(sizeof(Particle) * m_MaxQauds);
		m_ParticleVertexBuffer = CreateRef<StorageBuffer>(sizeof(ParticleVertex) * 4 * m_MaxQauds);
		m_CounterBuffer = CreateRef<StorageBuffer>(sizeof(CounterBuffer));

		m_Emitter.MaxParticles = 1025;
		m_Emitter.EmissionQuantity = 5;
		m_Emitter.EmitterPosition = glm::vec3(0);
		m_Emitter.EmitterDirection = glm::normalize(glm::vec3(-1.0f, -1.0f, 0.0f));

		m_EmitterUniformBuffer = CreateRef<UniformBuffer>(&m_Emitter, sizeof(Emitter));

		m_ComputeShader = CreateRef<Shader>("assets/shaders/compute.shader");
		m_ComputePipeline = CreateRef<VulkanComputePipeline>(m_ComputeShader);

		m_ParticleShader = CreateRef<Shader>("assets/shaders/particle.shader");

		auto renderer = Application::GetApp().GetRenderer();

		VertexBufferLayout* layout = new VertexBufferLayout({
			{ ShaderUniformType::FLOAT3, offsetof(ParticleVertex, Position) },
		});

		// TODO: Fix incorrect stride calculation for structs
		layout->SetStride(layout->GetStride() + sizeof(float));

		PipelineSpecification pipelineSpec;
		pipelineSpec.Shader = m_ParticleShader;
		pipelineSpec.TargetRenderPass = renderer->GetFramebuffer()->GetRenderPass();
		pipelineSpec.Layout = layout;

		m_Pipeline = CreateRef<VulkanPipeline>(pipelineSpec);

		delete layout;

		uint32_t* indices = new uint32_t[m_MaxIndices];
		uint32_t offset = 0;
		for (uint32_t i = 0; i < m_MaxIndices; i += 6)
		{
			indices[i + 0] = offset + 0;
			indices[i + 1] = offset + 1;
			indices[i + 2] = offset + 2;

			indices[i + 3] = offset + 2;
			indices[i + 4] = offset + 3;
			indices[i + 5] = offset + 0;

			offset += 4;
		}

		m_IndexBuffer = CreateRef<IndexBuffer>(indices, sizeof(uint32_t) * m_MaxIndices, m_MaxIndices);
		delete indices;

		// TODO: Auto generate write descriptors -- Move to shader?
		{
			VkWriteDescriptorSet& particleWriteDescriptor = m_ComputeWriteDescriptors.emplace_back();
			particleWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			particleWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			particleWriteDescriptor.dstBinding = 0;
			particleWriteDescriptor.pBufferInfo = &m_ParticleBuffer->getDescriptorBufferInfo();
			particleWriteDescriptor.descriptorCount = 1;

			VkWriteDescriptorSet& particleVertexWriteDescriptor = m_ComputeWriteDescriptors.emplace_back();
			particleVertexWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			particleVertexWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			particleVertexWriteDescriptor.dstBinding = 1;
			particleVertexWriteDescriptor.pBufferInfo = &m_ParticleVertexBuffer->getDescriptorBufferInfo();
			particleVertexWriteDescriptor.descriptorCount = 1;

			VkWriteDescriptorSet& emitterWriteDescriptor = m_ComputeWriteDescriptors.emplace_back();
			emitterWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			emitterWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			emitterWriteDescriptor.dstBinding = 2;
			emitterWriteDescriptor.pBufferInfo = &m_EmitterUniformBuffer->getDescriptorBufferInfo();
			emitterWriteDescriptor.descriptorCount = 1;

			VkWriteDescriptorSet& counterBufferWriteDescriptor = m_ComputeWriteDescriptors.emplace_back();
			counterBufferWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			counterBufferWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			counterBufferWriteDescriptor.dstBinding = 3;
			counterBufferWriteDescriptor.pBufferInfo = &m_CounterBuffer->getDescriptorBufferInfo();
			counterBufferWriteDescriptor.descriptorCount = 1;

			VkWriteDescriptorSet& cameraWriteDescriptor = m_ComputeWriteDescriptors.emplace_back();
			cameraWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			cameraWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			cameraWriteDescriptor.dstBinding = 4;
			cameraWriteDescriptor.pBufferInfo = &renderer->GetCameraUB()->getDescriptorBufferInfo();
			cameraWriteDescriptor.descriptorCount = 1;
		}

		{
			VkWriteDescriptorSet& particleWriteDescriptor = m_ParticleWriteDescriptors.emplace_back();
			particleWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			particleWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			particleWriteDescriptor.dstBinding = 0;
			particleWriteDescriptor.pBufferInfo = &m_ParticleBuffer->getDescriptorBufferInfo();
			particleWriteDescriptor.descriptorCount = 1;

			VkWriteDescriptorSet& cameraWriteDescriptor = m_ParticleWriteDescriptors.emplace_back();
			cameraWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			cameraWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			cameraWriteDescriptor.dstBinding = 1;
			cameraWriteDescriptor.pBufferInfo = &renderer->GetCameraUB()->getDescriptorBufferInfo();
			cameraWriteDescriptor.descriptorCount = 1;
		}

	}

	void ParticleLayer::OnUpdate()
	{
		m_Camera->Update();

		m_Emitter.Time = Application::GetApp().GetGlobalTime();
		m_Emitter.DeltaTime = Application::GetApp().GetDeltaTime();
		m_EmitterUniformBuffer->UpdateBuffer(&m_Emitter);

		auto renderer = Application::GetApp().GetRenderer();
		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();

		// TODO: Auto update descriptors -- Move to shader?
		{
			VkDescriptorSetAllocateInfo computeDescriptorSetAllocateInfo{};
			computeDescriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			computeDescriptorSetAllocateInfo.pSetLayouts = m_ComputeShader->GetDescriptorSetLayouts().data();
			computeDescriptorSetAllocateInfo.descriptorSetCount = m_ComputeShader->GetDescriptorSetLayouts().size();

			m_ComputeDescriptorSet = renderer->AllocateDescriptorSet(computeDescriptorSetAllocateInfo);

			for (auto& wd : m_ComputeWriteDescriptors)
				wd.dstSet = m_ComputeDescriptorSet;

			vkUpdateDescriptorSets(device->GetLogicalDevice(), m_ComputeWriteDescriptors.size(), m_ComputeWriteDescriptors.data(), 0, NULL);
		}

		// TODO: Move to compute queue
		VkCommandBuffer commandBuffer = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ComputePipeline->GetPipeline());
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ComputePipeline->GetPipelineLayout(), 0, 1, &m_ComputeDescriptorSet, 0, 0);
		vkCmdDispatch(commandBuffer, 1, 1, 1);

		device->FlushCommandBuffer(commandBuffer, true);

		m_ParticleCount = *m_CounterBuffer->Map<uint32_t>();
		m_CounterBuffer->Unmap();
	}

	void ParticleLayer::OnRender()
	{
		auto renderer = Application::GetApp().GetRenderer();
		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();
		VkCommandBuffer commandBuffer = renderer->GetActiveCommandBuffer();
		
		// Why does moving this code into update cause it to break
		{
			VkDescriptorSetAllocateInfo particleDescriptorSetAllocateInfo{};
			particleDescriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			particleDescriptorSetAllocateInfo.pSetLayouts = m_ParticleShader->GetDescriptorSetLayouts().data();
			particleDescriptorSetAllocateInfo.descriptorSetCount = m_ParticleShader->GetDescriptorSetLayouts().size();

			m_ParticleDescriptorSet = renderer->AllocateDescriptorSet(particleDescriptorSetAllocateInfo);

			for (auto& wd : m_ParticleWriteDescriptors)
				wd.dstSet = m_ParticleDescriptorSet;

			vkUpdateDescriptorSets(device->GetLogicalDevice(), m_ParticleWriteDescriptors.size(), m_ParticleWriteDescriptors.data(), 0, NULL);
		}

		renderer->BeginScene(m_Camera);
		renderer->BeginRenderPass(renderer->GetFramebuffer());

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetPipeline());

		VkDeviceSize offset = 0;
		VkBuffer vertexBuffer = m_ParticleVertexBuffer->GetBuffer();
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &offset);

		vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);;
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetPipelineLayout(), 0, 1, &m_ParticleDescriptorSet, 0, 0);
		vkCmdDrawIndexed(commandBuffer, m_ParticleCount * 6, 1, 0, 0, 0);

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