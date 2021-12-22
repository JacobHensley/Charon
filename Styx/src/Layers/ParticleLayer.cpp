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

		m_MaxIndices = m_MaxParticles * 6;

		m_ParticleBuffer = CreateRef<StorageBuffer>(sizeof(Particle) * m_MaxParticles);
		m_ParticleVertexBuffer = CreateRef<StorageBuffer>(sizeof(ParticleVertex) * 4 * m_MaxParticles, false, true);

		m_Emitter.Position = glm::vec3(0.0f);
		m_Emitter.Direction = normalize(glm::vec3(0.0f, 1.0f, 0.0f));
		m_Emitter.DirectionrRandomness = 2.0f;
		m_Emitter.VelocityRandomness = 0.0025f;
		m_Emitter.Gravity = 0.005f;
		m_Emitter.MaxParticles = 1025;
		m_Emitter.EmissionQuantity = 100;
		m_Emitter.Time = -10.0f;
		m_Emitter.DeltaTime= -8.5f;

		m_Emitter.InitialRotation = glm::vec3(0.0f);
		m_Emitter.InitialScale = glm::vec3(1.0f);
		m_Emitter.InitialLifetime = 3.0f;
		m_Emitter.InitialSpeed = 5.0f;
		m_Emitter.InitialColor = glm::vec3(1.0f, 0.0f, 0.0f);

		m_EmitterUniformBuffer = CreateRef<UniformBuffer>(&m_Emitter, sizeof(Emitter));

		m_ParticleShader = CreateRef<Shader>("assets/shaders/particle.shader");
		
		m_ShaderParticleBegin = CreateRef<Shader>("assets/shaders/ParticleBegin.shader");
		m_ShaderParticleEmit = CreateRef<Shader>("assets/shaders/ParticleEmit.shader");
		m_ShaderParticleSimulate = CreateRef<Shader>("assets/shaders/ParticleSimulate.shader");
		m_ShaderParticleEnd = CreateRef<Shader>("assets/shaders/ParticleEnd.shader");

		m_ParticlePipelines.Begin = CreateRef<VulkanComputePipeline>(m_ShaderParticleBegin);
		m_ParticlePipelines.Emit = CreateRef<VulkanComputePipeline>(m_ShaderParticleEmit);
		m_ParticlePipelines.Simulate = CreateRef<VulkanComputePipeline>(m_ShaderParticleSimulate);
		m_ParticlePipelines.End = CreateRef<VulkanComputePipeline>(m_ShaderParticleEnd);

		m_ParticleBuffers.AliveBufferPreSimulate = CreateRef<StorageBuffer>(sizeof(uint32_t) * m_MaxParticles);
		m_ParticleBuffers.AliveBufferPostSimulate = CreateRef<StorageBuffer>(sizeof(uint32_t) * m_MaxParticles);
		m_ParticleBuffers.DeadBuffer = CreateRef<StorageBuffer>(sizeof(uint32_t) * m_MaxParticles);
		m_ParticleBuffers.CounterBuffer = CreateRef<StorageBuffer>(sizeof(NewCounterBuffer));
		m_ParticleBuffers.IndirectDrawBuffer = CreateRef<StorageBuffer>(sizeof(IndirectDrawBuffer));
		m_ParticleBuffers.ParticleUniformEmitter = CreateRef<UniformBuffer>(&m_Emitter, sizeof(Emitter));

		// Init
		{
			ScopedMap<NewCounterBuffer, StorageBuffer> newCounterBuffer(m_ParticleBuffers.CounterBuffer);
			newCounterBuffer->DeadCount = m_MaxParticles;

			ScopedMap<uint32_t, StorageBuffer> deadBuffer(m_ParticleBuffers.DeadBuffer);
			for (uint32_t i = 0; i < m_MaxParticles; i++)
				deadBuffer[i] = i;
		}

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
		delete[] indices;

		// For rendering particles
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

			VkWriteDescriptorSet& aliveBufferWriteDescriptor = m_ParticleWriteDescriptors.emplace_back();
			aliveBufferWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			aliveBufferWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			aliveBufferWriteDescriptor.dstBinding = 2;
			aliveBufferWriteDescriptor.pBufferInfo = &m_ParticleBuffers.AliveBufferPostSimulate->getDescriptorBufferInfo();
			aliveBufferWriteDescriptor.descriptorCount = 1;
		}

		// New particle system
		{
			VkWriteDescriptorSet& particleBufferWriteDescriptor = m_NewParticleWriteDescriptors.emplace_back();
			particleBufferWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			particleBufferWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			particleBufferWriteDescriptor.dstBinding = 0;
			particleBufferWriteDescriptor.pBufferInfo = &m_ParticleBuffer->getDescriptorBufferInfo();
			particleBufferWriteDescriptor.descriptorCount = 1;

			VkWriteDescriptorSet& aliveBufferPreSimulateWD = m_NewParticleWriteDescriptors.emplace_back();
			aliveBufferPreSimulateWD.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			aliveBufferPreSimulateWD.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			aliveBufferPreSimulateWD.dstBinding = 1;
			aliveBufferPreSimulateWD.pBufferInfo = &m_ParticleBuffers.AliveBufferPreSimulate->getDescriptorBufferInfo();
			aliveBufferPreSimulateWD.descriptorCount = 1;

			VkWriteDescriptorSet& aliveBufferPostSimulateWD = m_NewParticleWriteDescriptors.emplace_back();
			aliveBufferPostSimulateWD.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			aliveBufferPostSimulateWD.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			aliveBufferPostSimulateWD.dstBinding = 2;
			aliveBufferPostSimulateWD.pBufferInfo = &m_ParticleBuffers.AliveBufferPostSimulate->getDescriptorBufferInfo();
			aliveBufferPostSimulateWD.descriptorCount = 1;

			VkWriteDescriptorSet& deadBufferWD = m_NewParticleWriteDescriptors.emplace_back();
			deadBufferWD.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			deadBufferWD.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			deadBufferWD.dstBinding = 3;
			deadBufferWD.pBufferInfo = &m_ParticleBuffers.DeadBuffer->getDescriptorBufferInfo();
			deadBufferWD.descriptorCount = 1;

			VkWriteDescriptorSet& vertexBufferWD = m_NewParticleWriteDescriptors.emplace_back();
			vertexBufferWD.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			vertexBufferWD.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			vertexBufferWD.dstBinding = 4;
			vertexBufferWD.pBufferInfo = &m_ParticleVertexBuffer->getDescriptorBufferInfo();
			vertexBufferWD.descriptorCount = 1;

			VkWriteDescriptorSet& counterBufferWD = m_NewParticleWriteDescriptors.emplace_back();
			counterBufferWD.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			counterBufferWD.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			counterBufferWD.dstBinding = 5;
			counterBufferWD.pBufferInfo = &m_ParticleBuffers.CounterBuffer->getDescriptorBufferInfo();
			counterBufferWD.descriptorCount = 1;

			VkWriteDescriptorSet& indirectBufferWD = m_NewParticleWriteDescriptors.emplace_back();
			indirectBufferWD.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			indirectBufferWD.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			indirectBufferWD.dstBinding = 6;
			indirectBufferWD.pBufferInfo = &m_ParticleBuffers.IndirectDrawBuffer->getDescriptorBufferInfo();
			indirectBufferWD.descriptorCount = 1;

			VkWriteDescriptorSet& particleEmitterBufferWD = m_NewParticleWriteDescriptors.emplace_back();
			particleEmitterBufferWD.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			particleEmitterBufferWD.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			particleEmitterBufferWD.dstBinding = 7;
			particleEmitterBufferWD.pBufferInfo = &m_ParticleBuffers.ParticleUniformEmitter->getDescriptorBufferInfo();
			particleEmitterBufferWD.descriptorCount = 1;

			VkWriteDescriptorSet& cameraBufferWD = m_NewParticleWriteDescriptors.emplace_back();
			cameraBufferWD.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			cameraBufferWD.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			cameraBufferWD.dstBinding = 8;
			cameraBufferWD.pBufferInfo = &renderer->GetCameraUB()->getDescriptorBufferInfo();
			cameraBufferWD.descriptorCount = 1;
		}
	}

	static int frame = 0;

	void ParticleLayer::OnUpdate()
	{
		m_Camera->Update();

		m_Emitter.Time = Application::GetApp().GetGlobalTime();
		m_Emitter.DeltaTime = Application::GetApp().GetDeltaTime();
		m_ParticleBuffers.ParticleUniformEmitter->UpdateBuffer(&m_Emitter);

		auto renderer = Application::GetApp().GetRenderer();
		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();

		if (m_NextFrame || true)
		{

			if(false){
				ScopedMap<NewCounterBuffer, StorageBuffer> newCounterBuffer(m_ParticleBuffers.CounterBuffer);
				if (newCounterBuffer->DeadCount < 1)
					return;
			}

			// New particle system
			{
				m_NewParticleWriteDescriptors[1].dstBinding = (frame % 2 == 0) ? 1 : 2;
				m_NewParticleWriteDescriptors[2].dstBinding = (frame % 2 == 0) ? 2 : 1;

				if (frame % 2 == 0)
					m_ParticleWriteDescriptors[2].pBufferInfo = &m_ParticleBuffers.AliveBufferPostSimulate->getDescriptorBufferInfo();
				else
					m_ParticleWriteDescriptors[2].pBufferInfo = &m_ParticleBuffers.AliveBufferPreSimulate->getDescriptorBufferInfo();

				frame++;
			}

			// Particle compute
			{
				VkDescriptorSetAllocateInfo computeDescriptorSetAllocateInfo{};
				computeDescriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
				computeDescriptorSetAllocateInfo.pSetLayouts = m_ShaderParticleBegin->GetDescriptorSetLayouts().data();
				computeDescriptorSetAllocateInfo.descriptorSetCount = m_ShaderParticleBegin->GetDescriptorSetLayouts().size();
				m_NewParticleDescriptorSet = renderer->AllocateDescriptorSet(computeDescriptorSetAllocateInfo);

				for (auto& wd : m_NewParticleWriteDescriptors)
					wd.dstSet = m_NewParticleDescriptorSet;

				vkUpdateDescriptorSets(device->GetLogicalDevice(), m_NewParticleWriteDescriptors.size(), m_NewParticleWriteDescriptors.data(), 0, NULL);

				// Particle Begin
				{
					VkCommandBuffer commandBuffer = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

					vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ParticlePipelines.Begin->GetPipeline());
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ParticlePipelines.Begin->GetPipelineLayout(), 0, 1, &m_NewParticleDescriptorSet, 0, 0);
					vkCmdDispatch(commandBuffer, 1, 1, 1);

					device->FlushCommandBuffer(commandBuffer, true);

					ScopedMap<NewCounterBuffer, StorageBuffer> newCounterBuffer(m_ParticleBuffers.CounterBuffer);
					m_DebugBuffers[0].m_NewCounterBuffer = *newCounterBuffer;

					ScopedMap<IndirectDrawBuffer, StorageBuffer> indirectDrawBuffer(m_ParticleBuffers.IndirectDrawBuffer);
					m_DebugBuffers[0].m_IndirectDrawBuffer = *indirectDrawBuffer;

					//ScopedMap<uint32_t, StorageBuffer> deadBuffer(m_ParticleBuffers.DeadBuffer);
					//memcpy(m_DebugBuffers[0].m_DeadBuffer, &(*deadBuffer), sizeof(uint32_t) * m_MaxParticles);
				}

				// Particle Emit
				{
					VkCommandBuffer commandBuffer = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

					vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ParticlePipelines.Emit->GetPipeline());
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ParticlePipelines.Emit->GetPipelineLayout(), 0, 1, &m_NewParticleDescriptorSet, 0, 0);

					VkDeviceSize offset = { 0 };
					vkCmdDispatchIndirect(commandBuffer, m_ParticleBuffers.IndirectDrawBuffer->GetBuffer(), offset);

					device->FlushCommandBuffer(commandBuffer, true);

					ScopedMap<NewCounterBuffer, StorageBuffer> newCounterBuffer(m_ParticleBuffers.CounterBuffer);
					m_DebugBuffers[1].m_NewCounterBuffer = *newCounterBuffer;

					ScopedMap<IndirectDrawBuffer, StorageBuffer> indirectDrawBuffer(m_ParticleBuffers.IndirectDrawBuffer);
					m_DebugBuffers[1].m_IndirectDrawBuffer = *indirectDrawBuffer;

					//ScopedMap<uint32_t, StorageBuffer> deadBuffer(m_ParticleBuffers.DeadBuffer);
					//memcpy(m_DebugBuffers[1].m_DeadBuffer, &(*deadBuffer), sizeof(uint32_t) * m_MaxParticles);
				}


				// Particle Simulate
				if (true) {
					VkCommandBuffer commandBuffer = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

					vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ParticlePipelines.Simulate->GetPipeline());
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ParticlePipelines.Simulate->GetPipelineLayout(), 0, 1, &m_NewParticleDescriptorSet, 0, 0);

					VkDeviceSize offset = { offsetof(IndirectDrawBuffer, DispatchSimulation) };
					vkCmdDispatchIndirect(commandBuffer, m_ParticleBuffers.IndirectDrawBuffer->GetBuffer(), offset);

					device->FlushCommandBuffer(commandBuffer, true);

					ScopedMap<NewCounterBuffer, StorageBuffer> newCounterBuffer(m_ParticleBuffers.CounterBuffer);
					m_DebugBuffers[2].m_NewCounterBuffer = *newCounterBuffer;

					ScopedMap<IndirectDrawBuffer, StorageBuffer> indirectDrawBuffer(m_ParticleBuffers.IndirectDrawBuffer);
					m_DebugBuffers[2].m_IndirectDrawBuffer = *indirectDrawBuffer;
				}

				// Particle End
				if (true) {
					VkCommandBuffer commandBuffer = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

					vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ParticlePipelines.End->GetPipeline());
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ParticlePipelines.End->GetPipelineLayout(), 0, 1, &m_NewParticleDescriptorSet, 0, 0);
					vkCmdDispatch(commandBuffer, 1, 1, 1);

					device->FlushCommandBuffer(commandBuffer, true);

					ScopedMap<NewCounterBuffer, StorageBuffer> newCounterBuffer(m_ParticleBuffers.CounterBuffer);
					m_DebugBuffers[3].m_NewCounterBuffer = *newCounterBuffer;

					ScopedMap<IndirectDrawBuffer, StorageBuffer> indirectDrawBuffer(m_ParticleBuffers.IndirectDrawBuffer);
					m_DebugBuffers[3].m_IndirectDrawBuffer = *indirectDrawBuffer;
				}
			}
			m_NextFrame = false;
		}

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

		VkDeviceSize indirectBufferOffset = { offsetof(IndirectDrawBuffer, DrawParticles) };
		//vkCmdDrawIndirect(commandBuffer, m_ParticleBuffers.IndirectDrawBuffer->GetBuffer(), indirectBufferOffset, 1, 16);
		vkCmdDrawIndexedIndirect(commandBuffer, m_ParticleBuffers.IndirectDrawBuffer->GetBuffer(), indirectBufferOffset, 1, 0);

		//vkCmdDrawIndexed(commandBuffer, m_MaxParticles * 6, 1, 0, 0, 0);

		renderer->EndRenderPass();

		renderer->BeginRenderPass();
		renderer->RenderUI();
		renderer->EndRenderPass();

		renderer->EndScene();
	}

	void ParticleLayer::DrawParticleBuffers(int index)
	{
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;
		if (ImGui::TreeNodeEx("NewCounterBuffer", flags))
		{
			ImGui::Text("AliveCount: %u", m_DebugBuffers[index].m_NewCounterBuffer.AliveCount);
			ImGui::Text("DeadCount: %u", m_DebugBuffers[index].m_NewCounterBuffer.DeadCount);
			ImGui::Text("RealEmitCount: %u", m_DebugBuffers[index].m_NewCounterBuffer.RealEmitCount);
			ImGui::Text("AliveCount_AfterSimulation: %u", m_DebugBuffers[index].m_NewCounterBuffer.AliveCount_AfterSimulation);

			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("IndirectDrawBuffer", flags))
		{
			ImGui::Text("DispatchEmit: %u, %u, %u", m_DebugBuffers[index].m_IndirectDrawBuffer.DispatchEmit.x,
				m_DebugBuffers[index].m_IndirectDrawBuffer.DispatchEmit.y, m_DebugBuffers[index].m_IndirectDrawBuffer.DispatchEmit.z);
			ImGui::Text("DispatchSimulation: %u, %u, %u", m_DebugBuffers[index].m_IndirectDrawBuffer.DispatchSimulation.x, m_DebugBuffers[index].m_IndirectDrawBuffer.DispatchSimulation.y,
				m_DebugBuffers[index].m_IndirectDrawBuffer.DispatchSimulation.z);
			ImGui::Text("DrawParticles: %u, %u, %u, %u",
				m_DebugBuffers[index].m_IndirectDrawBuffer.DrawParticles.x, m_DebugBuffers[index].m_IndirectDrawBuffer.DrawParticles.y,
				m_DebugBuffers[index].m_IndirectDrawBuffer.DrawParticles.z, m_DebugBuffers[index].m_IndirectDrawBuffer.DrawParticles.w);

			ImGui::TreePop();
		}

		if (false && ImGui::TreeNodeEx("IndirectDrawBuffer", flags))
		{
			for (size_t i = 0; i < 1000; i += 10)
			{
				ImGui::Text("%u  %u  %u  %u  %u  %u  %u  %u  %u  %u",
					m_DebugBuffers[index].m_DeadBuffer[i + 0],
					m_DebugBuffers[index].m_DeadBuffer[i + 1],
					m_DebugBuffers[index].m_DeadBuffer[i + 2],
					m_DebugBuffers[index].m_DeadBuffer[i + 3],
					m_DebugBuffers[index].m_DeadBuffer[i + 4],
					m_DebugBuffers[index].m_DeadBuffer[i + 5],
					m_DebugBuffers[index].m_DeadBuffer[i + 6],
					m_DebugBuffers[index].m_DeadBuffer[i + 7],
					m_DebugBuffers[index].m_DeadBuffer[i + 8],
					m_DebugBuffers[index].m_DeadBuffer[i + 9]
				);
			}

			ImGui::TreePop();
		}
	}

	void ParticleLayer::OnImGUIRender()
	{
		ImGui::Begin("Buffer Debug");

		if (ImGui::Button("Next Frame"))
			m_NextFrame = true;

		if (ImGui::Button("Dump particle buffer"))
		{
			std::cout << "==============================" << std::endl;
			std::cout << "Particle Buffer Dump" << std::endl;
			std::cout << "==============================" << std::endl;
			ScopedMap<Particle, StorageBuffer> particleBuffer(m_ParticleBuffer);
			for (size_t i = 0; i < m_MaxParticles; i++)
			{
				Particle& p = particleBuffer[i];

				glm::vec3 Position;
				float Lifetime;
				glm::vec3 Rotation;
				float Speed;
				glm::vec3 Scale;
				glm::vec3 Color;
				glm::vec3 Velocity;

				std::cout << "Particle " << i << std::endl;
				std::cout << "  Position: " << p.Position.x << ", " << p.Position.y << ", " << p.Position.z << std::endl;
			}
			std::cout << "==============================" << std::endl;
		}

		ImGuiTreeNodeFlags flags = 0;
		if (ImGui::TreeNodeEx("ParticleBegin", flags))
		{
			DrawParticleBuffers(0);
			ImGui::TreePop();
		}
		if (ImGui::TreeNodeEx("ParticleEmit", flags))
		{
			DrawParticleBuffers(1);
			ImGui::TreePop();
		}
		if (ImGui::TreeNodeEx("ParticleSimulate", flags))
		{
			DrawParticleBuffers(2);
			ImGui::TreePop();
		}
		if (ImGui::TreeNodeEx("ParticleEnd", flags))
		{
			DrawParticleBuffers(3);
			ImGui::TreePop();
		}

		ImGui::End();

		ImGui::Begin("Emitter Settings");

		ImGui::DragFloat3("Position", glm::value_ptr(m_Emitter.Position));
		ImGui::DragFloat3("Direction", glm::value_ptr(m_Emitter.Direction));
		m_Emitter.Direction = normalize(m_Emitter.Direction);
		ImGui::DragFloat("Directionr Randomness", &m_Emitter.DirectionrRandomness);
		ImGui::DragFloat("Velocity Randomness", &m_Emitter.VelocityRandomness);
		ImGui::DragFloat("Gravity", &m_Emitter.Gravity);
		ImGui::DragScalar("Emission Rate", ImGuiDataType_U32, &m_Emitter.EmissionQuantity);
		ImGui::DragFloat3("Initial Rotation", glm::value_ptr(m_Emitter.InitialRotation));
		ImGui::DragFloat3("Initial Scale", glm::value_ptr(m_Emitter.InitialScale));
		ImGui::DragFloat("Initial Lifetime", &m_Emitter.InitialLifetime);
		ImGui::DragFloat("Initial Speed", &m_Emitter.InitialSpeed);
		ImGui::ColorEdit3("Initial Color", glm::value_ptr(m_Emitter.InitialColor));

		ImGui::End();
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