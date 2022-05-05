#include "ParticleLayer.h"
#include "Charon/Core/Application.h"
#include "Charon/Graphics/Renderer.h"
#include "Charon/Graphics/SceneRenderer.h"
#include "Charon/ImGui/imgui_impl_vulkan_with_textures.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include <glm/gtc/type_ptr.hpp>

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
		m_ParticleSort = CreateRef<ParticleSort>();
		m_ParticleSort->Init(m_MaxParticles);

		// Emitter settings
		m_Emitter.Position = glm::vec3(0.0f);
		m_Emitter.Direction = normalize(glm::vec3(0.0f, 1.0f, 0.0f));
		m_Emitter.DirectionrRandomness = 0.0f;
		m_Emitter.VelocityRandomness = 0.0f;
		m_Emitter.Gravity = 0.005f;
		m_Emitter.MaxParticles = 10;
		m_Emitter.EmissionQuantity = 100;
		m_Emitter.Time = -10.0f;
		m_Emitter.DeltaTime= -8.5f;
		m_Emitter.InitialRotation = glm::vec3(0.0f);
		m_Emitter.InitialScale = glm::vec3(1.0f);
		m_Emitter.InitialLifetime = 3.0f;
		m_Emitter.InitialSpeed = 5.0f;
		m_Emitter.InitialColor = glm::vec3(1.0f, 0.0f, 0.0f);
		
		m_Count = 0;//50.0f;

		// Shaders
		m_ParticleShaders.Begin = CreateRef<Shader>("assets/shaders/particle/ParticleBegin.shader");
		m_ParticleShaders.Emit = CreateRef<Shader>("assets/shaders/particle/ParticleEmit.shader");
		m_ParticleShaders.Simulate = CreateRef<Shader>("assets/shaders/particle/ParticleSimulate.shader");
		m_ParticleShaders.End = CreateRef<Shader>("assets/shaders/particle/ParticleEnd.shader");

		// Pipelines
		m_ParticlePipelines.Begin = CreateRef<VulkanComputePipeline>(m_ParticleShaders.Begin);
		m_ParticlePipelines.Emit = CreateRef<VulkanComputePipeline>(m_ParticleShaders.Emit);
		m_ParticlePipelines.Simulate = CreateRef<VulkanComputePipeline>(m_ParticleShaders.Simulate);
		m_ParticlePipelines.End = CreateRef<VulkanComputePipeline>(m_ParticleShaders.End);

		// Buffers
		m_ParticleBuffers.ParticleBuffer = CreateRef<StorageBuffer>(sizeof(Particle) * m_MaxParticles);
		m_ParticleBuffers.EmitterBuffer = CreateRef<UniformBuffer>(&m_Emitter, sizeof(Emitter));
		m_ParticleBuffers.AliveBufferPreSimulate = CreateRef<StorageBuffer>(sizeof(uint32_t) * m_MaxParticles, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
		m_ParticleBuffers.AliveBufferPostSimulate = CreateRef<StorageBuffer>(sizeof(uint32_t) * m_MaxParticles, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
		m_ParticleBuffers.DeadBuffer = CreateRef<StorageBuffer>(sizeof(uint32_t) * m_MaxParticles);
		m_ParticleBuffers.CounterBuffer = CreateRef<StorageBuffer>(sizeof(CounterBuffer));
		m_ParticleBuffers.IndirectDrawBuffer = CreateRef<StorageBuffer>(sizeof(IndirectDrawBuffer), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
		m_ParticleBuffers.VertexBuffer = CreateRef<StorageBuffer>(sizeof(ParticleVertex) * 4 * m_MaxParticles, false, true);
		m_ParticleBuffers.CameraDistanceBuffer = CreateRef<StorageBuffer>(sizeof(uint32_t) * m_MaxParticles, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
		m_ParticleBuffers.IndexBuffer = CreateRef<IndexBuffer>(sizeof(uint32_t) * m_MaxIndices, m_MaxIndices);

		// Display
		m_Camera = CreateRef<Camera>(glm::perspectiveFov(glm::radians(45.0f), 1280.0f, 720.0f, 0.1f, 100.0f));
		m_ViewportPanel = CreateRef<ViewportPanel>();

		// Init buffers 
		{
			// Counter buffer
			ScopedMap<CounterBuffer, StorageBuffer> newCounterBuffer(m_ParticleBuffers.CounterBuffer);
			newCounterBuffer->DeadCount = m_MaxParticles;

			// Dead buffer
			ScopedMap<uint32_t, StorageBuffer> deadBuffer(m_ParticleBuffers.DeadBuffer);
			for (uint32_t i = 0; i < m_MaxParticles; i++)
				deadBuffer[i] = i;

			// Index buffer
			ScopedMap<uint32_t, IndexBuffer> indexBuffer(m_ParticleBuffers.IndexBuffer);
			uint32_t offset = 0;
			for (uint32_t i = 0; i < m_MaxIndices; i += 6)
			{
				indexBuffer[i + 0] = offset + 0;
				indexBuffer[i + 1] = offset + 1;
				indexBuffer[i + 2] = offset + 2;

				indexBuffer[i + 3] = offset + 2;
				indexBuffer[i + 4] = offset + 3;
				indexBuffer[i + 5] = offset + 0;

				offset += 4;
			}
		}

		auto renderer = Application::GetApp().GetRenderer();

		// Particle renderer pipeline
		{
			m_ParticleShader = CreateRef<Shader>("assets/shaders/particle/particle.shader");

			VertexBufferLayout* layout = new VertexBufferLayout({
				{ ShaderUniformType::FLOAT3, offsetof(ParticleVertex, Position) },
			});

			// TODO: Fix incorrect stride calculation for structs
			layout->SetStride(layout->GetStride() + sizeof(float));

			PipelineSpecification pipelineSpec;
			pipelineSpec.Shader = m_ParticleShader;
			pipelineSpec.TargetRenderPass = renderer->GetFramebuffer()->GetRenderPass();
			pipelineSpec.Layout = layout;

			m_ParticleRendererPipeline = CreateRef<VulkanPipeline>(pipelineSpec);

			delete layout;
		}

		// Particle renderer write descriptors
		{
			VkWriteDescriptorSet& particleWriteDescriptor = m_ParticleRendererWriteDescriptors.emplace_back();
			particleWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			particleWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			particleWriteDescriptor.dstBinding = 0;
			particleWriteDescriptor.pBufferInfo = &m_ParticleBuffers.ParticleBuffer->getDescriptorBufferInfo();
			particleWriteDescriptor.descriptorCount = 1;

			VkWriteDescriptorSet& aliveBufferWriteDescriptor = m_ParticleRendererWriteDescriptors.emplace_back();
			aliveBufferWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			aliveBufferWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			aliveBufferWriteDescriptor.dstBinding = 1;
			aliveBufferWriteDescriptor.pBufferInfo = &m_ParticleBuffers.AliveBufferPostSimulate->getDescriptorBufferInfo();
			aliveBufferWriteDescriptor.descriptorCount = 1;

			VkWriteDescriptorSet& cameraWriteDescriptor = m_ParticleRendererWriteDescriptors.emplace_back();
			cameraWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			cameraWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			cameraWriteDescriptor.dstBinding = 2;
			cameraWriteDescriptor.pBufferInfo = &renderer->GetCameraUB()->getDescriptorBufferInfo();
			cameraWriteDescriptor.descriptorCount = 1;
		}

		// Particle simulation write descriptors
		{
			VkWriteDescriptorSet& particleBufferWD = m_ParticleSimulationWriteDescriptors.emplace_back();
			particleBufferWD.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			particleBufferWD.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			particleBufferWD.dstBinding = 0;
			particleBufferWD.pBufferInfo = &m_ParticleBuffers.ParticleBuffer->getDescriptorBufferInfo();
			particleBufferWD.descriptorCount = 1;

			VkWriteDescriptorSet& aliveBufferPreSimulateWD = m_ParticleSimulationWriteDescriptors.emplace_back();
			aliveBufferPreSimulateWD.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			aliveBufferPreSimulateWD.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			aliveBufferPreSimulateWD.dstBinding = 1;
			aliveBufferPreSimulateWD.pBufferInfo = &m_ParticleBuffers.AliveBufferPreSimulate->getDescriptorBufferInfo();
			aliveBufferPreSimulateWD.descriptorCount = 1;

			VkWriteDescriptorSet& aliveBufferPostSimulateWD = m_ParticleSimulationWriteDescriptors.emplace_back();
			aliveBufferPostSimulateWD.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			aliveBufferPostSimulateWD.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			aliveBufferPostSimulateWD.dstBinding = 2;
			aliveBufferPostSimulateWD.pBufferInfo = &m_ParticleBuffers.AliveBufferPostSimulate->getDescriptorBufferInfo();
			aliveBufferPostSimulateWD.descriptorCount = 1;

			VkWriteDescriptorSet& deadBufferWD = m_ParticleSimulationWriteDescriptors.emplace_back();
			deadBufferWD.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			deadBufferWD.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			deadBufferWD.dstBinding = 3;
			deadBufferWD.pBufferInfo = &m_ParticleBuffers.DeadBuffer->getDescriptorBufferInfo();
			deadBufferWD.descriptorCount = 1;

			VkWriteDescriptorSet& vertexBufferWD = m_ParticleSimulationWriteDescriptors.emplace_back();
			vertexBufferWD.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			vertexBufferWD.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			vertexBufferWD.dstBinding = 4;
			vertexBufferWD.pBufferInfo = &m_ParticleBuffers.VertexBuffer->getDescriptorBufferInfo();
			vertexBufferWD.descriptorCount = 1;

			VkWriteDescriptorSet& counterBufferWD = m_ParticleSimulationWriteDescriptors.emplace_back();
			counterBufferWD.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			counterBufferWD.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			counterBufferWD.dstBinding = 5;
			counterBufferWD.pBufferInfo = &m_ParticleBuffers.CounterBuffer->getDescriptorBufferInfo();
			counterBufferWD.descriptorCount = 1;

			VkWriteDescriptorSet& indirectBufferWD = m_ParticleSimulationWriteDescriptors.emplace_back();
			indirectBufferWD.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			indirectBufferWD.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			indirectBufferWD.dstBinding = 6;
			indirectBufferWD.pBufferInfo = &m_ParticleBuffers.IndirectDrawBuffer->getDescriptorBufferInfo();
			indirectBufferWD.descriptorCount = 1;

			VkWriteDescriptorSet& emitterBufferWD = m_ParticleSimulationWriteDescriptors.emplace_back();
			emitterBufferWD.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			emitterBufferWD.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			emitterBufferWD.dstBinding = 7;
			emitterBufferWD.pBufferInfo = &m_ParticleBuffers.EmitterBuffer->getDescriptorBufferInfo();
			emitterBufferWD.descriptorCount = 1;

			VkWriteDescriptorSet& cameraBufferWD = m_ParticleSimulationWriteDescriptors.emplace_back();
			cameraBufferWD.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			cameraBufferWD.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			cameraBufferWD.dstBinding = 8;
			cameraBufferWD.pBufferInfo = &renderer->GetCameraUB()->getDescriptorBufferInfo();
			cameraBufferWD.descriptorCount = 1;

			VkWriteDescriptorSet& cameraDistanceBufferWD = m_ParticleSimulationWriteDescriptors.emplace_back();
			cameraDistanceBufferWD.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			cameraDistanceBufferWD.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			cameraDistanceBufferWD.dstBinding = 9;
			cameraDistanceBufferWD.pBufferInfo = &m_ParticleBuffers.CameraDistanceBuffer->getDescriptorBufferInfo();
			cameraDistanceBufferWD.descriptorCount = 1;
		}
	}

	static int frame = 0;

	void ParticleLayer::OnUpdate()
	{
		auto renderer = Application::GetApp().GetRenderer();
		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();

		m_Camera->Update();

		// Skip if paused 
		if (m_Pause && !m_NextFrame)
			return;
		m_NextFrame = false;

		// Upload gradient points
		const auto& marks = m_ColorLifetimeGradient.getMarks();
		m_Emitter.GradientPointCount = marks.size();
		for (size_t i = 0; i < marks.size(); i++)
		{
			m_Emitter.ColorGradientPoints[i] = *(GradientPoint*)marks[i];
		}
			
		// Set global and delta time
		m_Emitter.Time = Application::GetApp().GetGlobalTime();
		m_Emitter.DeltaTime = Application::GetApp().GetDeltaTime();

		// TODO: Fix issue with lower particle per second count (TODO: Clean)
		// Set emission quantity based on particles per second and delta time
		m_EmitCount = std::max(0.0f, m_EmitCount - std::floor(m_EmitCount));
		m_EmitCount += m_Count * m_Emitter.DeltaTime;
		m_EmitCount += m_Burst;
		m_Emitter.EmissionQuantity = (uint32_t)m_EmitCount;
		m_Burst = 0.0f;

		// Emit 10 (TODO: Clean)
		if (false) {
			if (m_Emit10)
			{
				m_Emitter.EmissionQuantity = 10;
				m_Emit10 = false;
			}
			else
			{
				m_Emitter.EmissionQuantity = 0;
			}
		}

		// Upload emitter buffer
		m_ParticleBuffers.EmitterBuffer->UpdateBuffer(&m_Emitter);
		
		// Calculate particles per second (TODO: Clean)
		static int emissionCount = 0;
		emissionCount += m_Emitter.EmissionQuantity;
		m_SecondTimer -= m_Emitter.DeltaTime;
		if (m_SecondTimer <= 0.0f)
		{
			m_ParticlesEmittedPerSecond = emissionCount;
			emissionCount = 0;
			m_SecondTimer = 1.0f;
		}

		// Calculate burst (TODO: Clean)
		m_BurstIntervalCount -= m_Emitter.DeltaTime;
		if (m_BurstIntervalCount <= 0.0f && m_BurstCount > 0)
		{
			m_Burst += m_BurstCount;
			m_BurstIntervalCount = m_BurstInterval;
		}

		Ref<StorageBuffer> activeParticleBuffer;

		// Swap alive lists
		{
			// Swap alive list bindings
			m_ParticleSimulationWriteDescriptors[1].dstBinding = (frame % 2 == 0) ? 1 : 2;
			m_ParticleSimulationWriteDescriptors[2].dstBinding = (frame % 2 == 0) ? 2 : 1;

			// Select correct alive list to set a active list for drawing
			activeParticleBuffer = frame % 2 == 0 ? m_ParticleBuffers.AliveBufferPostSimulate : m_ParticleBuffers.AliveBufferPreSimulate;

			frame++;
		}

		// Update particle simulation write descriptors
		{
			VkDescriptorSetAllocateInfo computeDescriptorSetAllocateInfo{};
			computeDescriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			computeDescriptorSetAllocateInfo.pSetLayouts = m_ParticleShaders.Begin->GetDescriptorSetLayouts().data();
			computeDescriptorSetAllocateInfo.descriptorSetCount = m_ParticleShaders.Begin->GetDescriptorSetLayouts().size();
			m_ParticleSimulationDescriptorSet = renderer->AllocateDescriptorSet(computeDescriptorSetAllocateInfo);

			for (auto& wd : m_ParticleSimulationWriteDescriptors)
				wd.dstSet = m_ParticleSimulationDescriptorSet;

			vkUpdateDescriptorSets(device->GetLogicalDevice(), m_ParticleSimulationWriteDescriptors.size(), m_ParticleSimulationWriteDescriptors.data(), 0, NULL);
		}

		// Particle compute
		{
			{
				// Particle Begin
				{
					VkCommandBuffer commandBuffer = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

					vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ParticlePipelines.Begin->GetPipeline());
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ParticlePipelines.Begin->GetPipelineLayout(), 0, 1, &m_ParticleSimulationDescriptorSet, 0, 0);
					vkCmdDispatch(commandBuffer, 1, 1, 1);

					device->FlushCommandBuffer(commandBuffer, true);
				}

				// Particle Emit
				{
					VkCommandBuffer commandBuffer = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

					vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ParticlePipelines.Emit->GetPipeline());
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ParticlePipelines.Emit->GetPipelineLayout(), 0, 1, &m_ParticleSimulationDescriptorSet, 0, 0);

					VkDeviceSize offset = { 0 };
					vkCmdDispatchIndirect(commandBuffer, m_ParticleBuffers.IndirectDrawBuffer->GetBuffer(), offset);
					//vkCmdDispatch(commandBuffer, 1, 1, 1);

					device->FlushCommandBuffer(commandBuffer, true);
				}

				// Particle Simulate
				{
					VkCommandBuffer commandBuffer = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

					vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ParticlePipelines.Simulate->GetPipeline());
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ParticlePipelines.Simulate->GetPipelineLayout(), 0, 1, &m_ParticleSimulationDescriptorSet, 0, 0);

					VkDeviceSize offset = { offsetof(IndirectDrawBuffer, DispatchSimulation) };
					vkCmdDispatchIndirect(commandBuffer, m_ParticleBuffers.IndirectDrawBuffer->GetBuffer(), offset);
					//vkCmdDispatch(commandBuffer, 1, 1, 1);

					device->FlushCommandBuffer(commandBuffer, true);
				}

				// Particle End
				{
					VkCommandBuffer commandBuffer = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

					vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ParticlePipelines.End->GetPipeline());
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ParticlePipelines.End->GetPipelineLayout(), 0, 1, &m_ParticleSimulationDescriptorSet, 0, 0);
					vkCmdDispatch(commandBuffer, 1, 1, 1);

					device->FlushCommandBuffer(commandBuffer, true);
				}

			}

			{
			//	ScopedMap<CounterBuffer, StorageBuffer> buffer0(m_ParticleBuffers.CounterBuffer);

				// Sorting
				if (m_EnableSorting)
					m_DrawParticleIndexBuffer = m_ParticleSort->Sort(m_MaxParticles, m_ParticleBuffers.CameraDistanceBuffer, activeParticleBuffer);
				else
					m_DrawParticleIndexBuffer = activeParticleBuffer;
			}


			// Set alive post simulion buffer in particle rendering to correct index buffer
			m_ParticleRendererWriteDescriptors[1].pBufferInfo = &m_DrawParticleIndexBuffer->getDescriptorBufferInfo();
		}
	}

	void ParticleLayer::OnRender()
	{
		auto renderer = Application::GetApp().GetRenderer();
		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();
		VkCommandBuffer commandBuffer = renderer->GetActiveCommandBuffer();
		
		// Update particle renderer write descriptors
		{
			VkDescriptorSetAllocateInfo particleDescriptorSetAllocateInfo{};
			particleDescriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			particleDescriptorSetAllocateInfo.pSetLayouts = m_ParticleShader->GetDescriptorSetLayouts().data();
			particleDescriptorSetAllocateInfo.descriptorSetCount = m_ParticleShader->GetDescriptorSetLayouts().size();

			m_ParticleRendererDescriptorSet = renderer->AllocateDescriptorSet(particleDescriptorSetAllocateInfo);

			for (auto& wd : m_ParticleRendererWriteDescriptors)
				wd.dstSet = m_ParticleRendererDescriptorSet;

			vkUpdateDescriptorSets(device->GetLogicalDevice(), m_ParticleRendererWriteDescriptors.size(), m_ParticleRendererWriteDescriptors.data(), 0, NULL);
		}

		renderer->BeginScene(m_Camera);

		// Render particles
		{
			renderer->BeginRenderPass(renderer->GetFramebuffer());

			{
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ParticleRendererPipeline->GetPipeline());

				VkDeviceSize offset = 0;
				VkBuffer vertexBuffer = m_ParticleBuffers.VertexBuffer->GetBuffer();
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &offset);

				vkCmdBindIndexBuffer(commandBuffer, m_ParticleBuffers.IndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);;
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ParticleRendererPipeline->GetPipelineLayout(), 0, 1, &m_ParticleRendererDescriptorSet, 0, 0);

				VkDeviceSize indirectBufferOffset = { offsetof(IndirectDrawBuffer, DrawParticles) };
				vkCmdDrawIndexedIndirect(commandBuffer, m_ParticleBuffers.IndirectDrawBuffer->GetBuffer(), indirectBufferOffset, 1, 0);
			}

			renderer->EndRenderPass();
		}

		// Render UI
		{
			renderer->BeginRenderPass();
			renderer->RenderUI();
			renderer->EndRenderPass();
		}

		renderer->EndScene();
	}

	void ParticleLayer::OnImGUIRender()
	{
		m_ViewportPanel->Render(m_Camera);

		// Particle Settings Panel
		{
			ImGui::Begin("Particle settings");

			ImGui::Checkbox("Sort", &m_EnableSorting);
			if (ImGui::Button("Emit 10"))
				m_Emit10 = true;

			if (ImGui::CollapsingHeader("Stats"), ImGuiTreeNodeFlags_DefaultOpen)
			{
				ImGui::Text("Particle Count: %d", m_ParticleCount);
			}

			if (m_Pause)
			{
				if (ImGui::Button("Play"))
					m_Pause = false;
				ImGui::SameLine();
				if (ImGui::Button("Next Frame"))
					m_NextFrame = true;
			}
			else
			{
				if (ImGui::Button("Pause"))
					m_Pause = true;
			}

			if (ImGui::Button("Dump Particle Buffer"))
			{
				ScopedMap<Particle, StorageBuffer> buffer0(m_ParticleBuffers.ParticleBuffer);
				for (int i = 0; i < m_MaxParticles; i++)
				{
					std::cout << "[" << i << "]" << buffer0->Position.z << '\n';
				}
			}

			if (ImGui::Button("Dump Buffer 0"))
			{
				ScopedMap<uint32_t, StorageBuffer> buffer0(m_ParticleBuffers.AliveBufferPreSimulate);
				for (int i = 0; i < m_MaxParticles; i+=4)
				{
					std::cout << buffer0[i] << ' ' << buffer0[i+1] << ' ' << buffer0[i + 2] << ' ' << buffer0[i + 3] << '\n';
				}
			}

			if (ImGui::Button("Dump Buffer 1"))
			{
				ScopedMap<uint32_t, StorageBuffer> buffer0(m_ParticleBuffers.AliveBufferPostSimulate);
				for (int i = 0; i < m_MaxParticles; i += 4)
				{
					std::cout << buffer0[i] << ' ' << buffer0[i + 1] << ' ' << buffer0[i + 2] << ' ' << buffer0[i + 3] << '\n';
				}
			}

			if (ImGui::Button("Dump Camera Distance Buffer "))
			{
				ScopedMap<uint32_t, StorageBuffer> buffer0(m_ParticleBuffers.CameraDistanceBuffer);
				for (int i = 0; i < m_MaxParticles; i++)
				{
					std::cout << buffer0[i] << '\n';
				}
			}

			if (ImGui::CollapsingHeader("Initial Settings"), ImGuiTreeNodeFlags_DefaultOpen)
			{
				ImGui::DragFloat3("Initial Rotation", glm::value_ptr(m_Emitter.InitialRotation), 0.1f);
				ImGui::DragFloat3("Initial Scale", glm::value_ptr(m_Emitter.InitialScale), 0.1f, 0.0f);
				ImGui::DragFloat3("Initial Color", glm::value_ptr(m_Emitter.InitialColor), 0.1f, 0.0f, 1.0f);
				ImGui::DragFloat("Initial Spped", &m_Emitter.InitialSpeed, 0.1f);
				ImGui::DragFloat("Initial Lifetime", &m_Emitter.InitialLifetime, 0.1f);
				ImGui::DragFloat("Gravity Modifier", &m_Emitter.Gravity, 0.1f);
			}

			if (ImGui::CollapsingHeader("Emitter Settings"), ImGuiTreeNodeFlags_DefaultOpen)
			{
				ImGui::DragFloat3("Emitter Postion", glm::value_ptr(m_Emitter.Position), 0.1f);
				ImGui::DragFloat("Emission Quantity", &m_Count, 0.1f, 0.0f);
				ImGui::DragFloat("Burst Count", &m_BurstCount, 0.1f, 0.0f);
				ImGui::DragFloat("Burst Interval", &m_BurstInterval, 0.1f, 0.0f);
			}

			if (ImGui::CollapsingHeader("Over-Lifetime Settings"), ImGuiTreeNodeFlags_DefaultOpen)
			{
				static ImGradientMark* draggingMark = nullptr;
				static ImGradientMark* selectedMark = nullptr;
				bool isDragging = false;

				// TODO: Add way to disable widget
				bool updated = GradientEditor(&m_ColorLifetimeGradient, "Color Over Lifetime", draggingMark, selectedMark, isDragging);
			}

			ImGui::End();
		}

	}

}