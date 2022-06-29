#define IMGUI_DEFINE_MATH_OPERATORS

#include "ParticleLayer.h"
#include "Charon/Core/Application.h"
#include "Charon/Graphics/Renderer.h"
#include "Charon/Graphics/SceneRenderer.h"
#include "Charon/ImGui/imgui_impl_vulkan_with_textures.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include <glm/gtc/type_ptr.hpp>

namespace Charon {

	using namespace ImGui;

	static float s_GraphTime = 0.0f;

	int CurveEditor(const char* label
		, float* values
		, int points_count
		, const ImVec2& editor_size
		, ImU32 flags
		, int* new_count);

	enum class CurveEditorFlags
	{
		NO_TANGENTS = 1 << 0,
		SHOW_GRID = 1 << 1,
		RESET = 1 << 2
	};

	static const float NODE_SLOT_RADIUS = 4.0f;

	static glm::vec2 BezierCubicCalc(const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& p3, const glm::vec2& p4, float t)
	{
		float u = 1.0f - t;
		float w1 = u * u * u;
		float w2 = 3 * u * u * t;
		float w3 = 3 * u * t * t;
		float w4 = t * t * t;
		return glm::vec2(w1 * p1.x + w2 * p2.x + w3 * p3.x + w4 * p4.x, w1 * p1.y + w2 * p2.y + w3 * p3.y + w4 * p4.y);
	}

	static glm::vec2 BezierCubicCalcIm(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, float t)
	{
		return BezierCubicCalc(*(glm::vec2*)&p1, *(glm::vec2*)&p2, *(glm::vec2*)&p3, *(glm::vec2*)&p4, t);
	}

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

		m_DebugSphere = CreateRef<Mesh>("assets/models/Sphere.gltf");
		m_Plane20m = CreateRef<Mesh>("assets/models/Plane20m.gltf");

		// Emitter settings
		m_Emitter.Position = glm::vec3(0.0f);
		m_Emitter.Direction = normalize(glm::vec3(1.0f, -0.5f, 0.0f));
		m_Emitter.DirectionRandomness = 1.0f;
		m_Emitter.VelocityRandomness = 1.0f;
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
		
		m_RequestedParticlesPerSecond = 0;
		m_RequestedParticlesPerFrame = 0;
		
		m_RequestedParticlesPerSecond = 5;
		m_Emitter.Position = glm::vec3(0.0f, 5.0f, 0.0f);
		m_Emitter.Gravity = 5.0f;

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
		m_ParticleBuffers.ParticleDrawDetails = CreateRef<UniformBuffer>(&m_ParticleDrawDetails, sizeof(ParticleDrawDetails));
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
			pipelineSpec.WriteDepth = false;

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

			VkWriteDescriptorSet& particleDrawDetailsWriteDescriptor = m_ParticleRendererWriteDescriptors.emplace_back();
			particleDrawDetailsWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			particleDrawDetailsWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			particleDrawDetailsWriteDescriptor.dstBinding = 3;
			particleDrawDetailsWriteDescriptor.pBufferInfo = &m_ParticleBuffers.ParticleDrawDetails->getDescriptorBufferInfo();
			particleDrawDetailsWriteDescriptor.descriptorCount = 1;

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

			VkWriteDescriptorSet& depthTextureWD = m_ParticleSimulationWriteDescriptors.emplace_back();
			depthTextureWD.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			depthTextureWD.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			depthTextureWD.dstBinding = 10;
			depthTextureWD.pImageInfo = &renderer->GetFramebuffer()->GetDepthImage()->GetDescriptorImageInfo();
			depthTextureWD.descriptorCount = 1;
		}
	}

	static int frame = 0;

	void ParticleLayer::OnUpdate()
	{
		auto renderer = Application::GetApp().GetRenderer();
		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();
		static bool first = true;
		if (first)
		{
			VkCommandBuffer commandBuffer = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			renderer->SetActiveCommandBuffer(commandBuffer);
			renderer->BeginRenderPass(renderer->GetFramebuffer(), true);
			renderer->EndRenderPass();
			device->FlushCommandBuffer(commandBuffer, true);
			first = false;
		}


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

		s_GraphTime += m_Emitter.DeltaTime;
		if (s_GraphTime > 2.0f)
			s_GraphTime = 0.0f;

		// Set emission quantity based on particles per second and delta time
		m_RequestedParticlesPerFrame = std::max(0.0f, m_RequestedParticlesPerFrame - std::floor(m_RequestedParticlesPerFrame));
		m_RequestedParticlesPerFrame += m_RequestedParticlesPerSecond * m_Emitter.DeltaTime;
		m_Emitter.EmissionQuantity = (uint32_t)m_RequestedParticlesPerFrame;

		// Upload emitter buffer
		m_ParticleBuffers.EmitterBuffer->UpdateBuffer(&m_Emitter);
		
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

					device->FlushCommandBuffer(commandBuffer, true);
				}

				// Particle Simulate
				{
					VkCommandBuffer commandBuffer = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

					vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ParticlePipelines.Simulate->GetPipeline());
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ParticlePipelines.Simulate->GetPipelineLayout(), 0, 1, &m_ParticleSimulationDescriptorSet, 0, 0);

					VkDeviceSize offset = { offsetof(IndirectDrawBuffer, DispatchSimulation) };
					vkCmdDispatchIndirect(commandBuffer, m_ParticleBuffers.IndirectDrawBuffer->GetBuffer(), offset);

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

			// Sorting
			{
				ScopedMap<CounterBuffer, StorageBuffer> counterBuffer(m_ParticleBuffers.CounterBuffer);
				uint32_t activeParticleCount = counterBuffer->AliveCount_AfterSimulation;

				if (m_EnableSorting && activeParticleCount > 0)
				{
					m_ParticleBuffers.DrawParticleIndexBuffer = m_ParticleSort->Sort(activeParticleCount, m_ParticleBuffers.CameraDistanceBuffer, activeParticleBuffer);
					m_ParticleDrawDetails.IndexOffset = m_MaxParticles - activeParticleCount;
				}
				else
				{
					m_ParticleBuffers.DrawParticleIndexBuffer = activeParticleBuffer;
					m_ParticleDrawDetails.IndexOffset = 0;
				}
	
				m_ParticleBuffers.ParticleDrawDetails->UpdateBuffer(&m_ParticleDrawDetails);
			}

			// Set alive post simulion buffer in particle rendering to correct index buffer
			m_ParticleRendererWriteDescriptors[1].pBufferInfo = &m_ParticleBuffers.DrawParticleIndexBuffer->getDescriptorBufferInfo();
		}
	}

	int ParticleLayer::GetGraphIndex(const std::vector<glm::vec2>& bezierCubicPoints, float x)
	{
		for (int i = 0; i < bezierCubicPoints.size(); i += 4)
		{
			const glm::vec2& p0 = bezierCubicPoints[i];
			const glm::vec2& p1 = bezierCubicPoints[i + 3];
			if (x >= p0.x && x <= p1.x)
				return i;
		}
		return -1;
	}

	void ParticleLayer::OnRender()
	{
		auto renderer = Application::GetApp().GetRenderer();
		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();
		VkCommandBuffer commandBuffer = renderer->GetActiveCommandBuffer();
		
		// Debug quad

		float graphTime = s_GraphTime * 0.5f;

		int graphIndex = GetGraphIndex(m_BezierCubicPoints, graphTime);
		glm::vec3 spherePos(0.0f);
		if (graphIndex != -1)
		{
			glm::vec2 p0 = m_BezierCubicPoints[graphIndex];
			glm::vec2 p1 = m_BezierCubicPoints[graphIndex + 3];

			glm::vec2 nextPoint(0.0f);
			if (m_BezierCubicPoints.size() > (graphIndex + 4))
				nextPoint = m_BezierCubicPoints[graphIndex + 4];
			
			// NOTE: keep this?
			float distance = std::min(abs(p1.x - nextPoint.x), abs(p1.x - p0.x));

			glm::vec2 t0 = p0 + m_BezierCubicPoints[graphIndex + 1] * distance;
			glm::vec2 t1 = p1 + m_BezierCubicPoints[graphIndex + 2] * distance;

			glm::vec2 pointOnGraph = BezierCubicCalc(p0, t0, t1, p1, (graphTime - p0.x) / (p1.x - p0.x));

			float scale = 4.0f;
			spherePos = glm::vec3(graphTime * scale, pointOnGraph.y * scale, 0.0f);
		}
		//renderer->SubmitMesh(m_DebugSphere, glm::translate(glm::mat4(1.0f), spherePos));
		renderer->SubmitMesh(m_Plane20m, glm::mat4(1.0f));

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
			// Geo
			renderer->BeginRenderPass(renderer->GetFramebuffer(), true);
			renderer->Render();
			renderer->EndRenderPass();

			// depth texture!

			// Particles (no clear)
			renderer->BeginRenderPass(renderer->GetFramebuffer());
			{
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ParticleRendererPipeline->GetPipeline());

				VkDeviceSize offset = 0;
				VkBuffer vertexBuffer = m_ParticleBuffers.VertexBuffer->GetBuffer();
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &offset);

				vkCmdBindIndexBuffer(commandBuffer, m_ParticleBuffers.IndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ParticleRendererPipeline->GetPipelineLayout(), 0, 1, &m_ParticleRendererDescriptorSet, 0, 0);

				VkDeviceSize indirectBufferOffset = { offsetof(IndirectDrawBuffer, DrawParticles) };
				vkCmdDrawIndexedIndirect(commandBuffer, m_ParticleBuffers.IndirectDrawBuffer->GetBuffer(), indirectBufferOffset, 1, 0);
			}

			renderer->Render();
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

			{
				auto renderer = Application::GetApp().GetRenderer();
				auto depthImage = renderer->GetFramebuffer()->GetDepthImage();

				const auto& descriptorInfo = depthImage->GetDescriptorImageInfo();
				ImTextureID textureID = ImGui_ImplVulkan_AddTexture(descriptorInfo.sampler, descriptorInfo.imageView, descriptorInfo.imageLayout);
				ImGui::Image((void*)textureID, { 512, 512 }, ImVec2(0, 1), ImVec2(1, 0));
			}

			ScopedMap<CounterBuffer, StorageBuffer> counterBuffer(m_ParticleBuffers.CounterBuffer);
			ImGui::Text("Alive Particles: %u", counterBuffer->AliveCount_AfterSimulation);

			ImGui::NewLine();

			if (ImGui::CollapsingHeader("Initial Settings", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::DragFloat3("Initial Rotation", glm::value_ptr(m_Emitter.InitialRotation), 0.1f);
				ImGui::DragFloat3("Initial Scale", glm::value_ptr(m_Emitter.InitialScale), 0.1f, 0.0f);
				ImGui::DragFloat3("Initial Color", glm::value_ptr(m_Emitter.InitialColor), 0.1f, 0.0f, 1.0f);
				ImGui::DragFloat("Initial Spped", &m_Emitter.InitialSpeed, 0.1f);
				ImGui::DragFloat("Initial Lifetime", &m_Emitter.InitialLifetime, 0.1f);
				ImGui::DragFloat("Gravity Modifier", &m_Emitter.Gravity, 0.1f);
			}

			if (ImGui::CollapsingHeader("Emitter Settings", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::DragFloat3("Emitter Postion", glm::value_ptr(m_Emitter.Position), 0.1f);
				ImGui::DragFloat("Emission Quantity", &m_RequestedParticlesPerSecond, 0.1f, 0.0f);
			}

			if (ImGui::CollapsingHeader("Over-Lifetime Settings", ImGuiTreeNodeFlags_DefaultOpen))
			{
				static ImGradientMark* draggingMark = nullptr;
				static ImGradientMark* selectedMark = nullptr;
				bool isDragging = false;

				bool updated = GradientEditor(&m_ColorLifetimeGradient, "Color Over Lifetime", draggingMark, selectedMark, isDragging);

				static ImVec2 values[12];
				static int count = 2;
				static bool first = true;
				static int newCount;
				if (count != newCount)
					count = newCount;

				if (count < 2)
					count = 2;

				if (first)
				{
					values[0] = ImVec2(0.0f, 0.0f); values[1] = ImVec2(0.0f, 0.0f); values[2] = ImVec2(1.0f, 0.0f);
					values[3] = ImVec2(-1.0f, 0.0f); values[4] = ImVec2(1.0f, 1.0f); values[5] = ImVec2(0.0f, 0.0f);
					first = false;
				}

				CurveEditor("Size", (float*)values, count, ImVec2(400, 200), (ImU32)CurveEditorFlags::SHOW_GRID, &newCount);

				// FIRST -> POINT = values[1], TANG = values[2]
				// LAST -> POINT = values[4], TANG = values[3], values[5]

				// FIRST -> POINT = values[1], TANG = values[2]
				// MIDDLE -> POINT = values[4], TANG = values[3], values[5]
				// LAST -> POINT = values[7], TANG = values[6]

				m_BezierCubicPoints.clear();

				int pointsCount = newCount;
				if (pointsCount >= 2)
				{
					// FIRST point
					ImVec2 firstPoint = values[1]; // values[1]
					ImVec2 firstTangent = values[2]; // values[2]

					m_BezierCubicPoints.emplace_back(firstPoint.x, firstPoint.y);
					m_BezierCubicPoints.emplace_back(firstTangent.x, firstTangent.y);
					m_Emitter.VelocityCurvePoints[0] = glm::vec4(firstPoint.x, firstPoint.y, firstTangent.x, firstTangent.y);

					int pointIndex = 1;
					// if > 2 points
					for (; pointIndex < pointsCount - 1; pointIndex++)
					{
						ImVec2* points = ((ImVec2*)values) + pointIndex * 3;

						ImVec2 tangLeft = points[0];
						ImVec2 point = points[1];
						ImVec2 tangRight = points[2];
						
						
						m_BezierCubicPoints.emplace_back(tangLeft.x, tangLeft.y);
						m_BezierCubicPoints.emplace_back(point.x, point.y);

						m_BezierCubicPoints.emplace_back(point.x, point.y);
						m_BezierCubicPoints.emplace_back(tangRight.x, tangRight.y);

						m_Emitter.VelocityCurvePoints[pointIndex] = glm::vec4(tangLeft.x, tangLeft.y, point.x, point.y);
						m_Emitter.VelocityCurvePoints[pointIndex] = glm::vec4(point.x, point.y, tangRight.x, tangRight.y);
					}

					// LAST point
					ImVec2 lastPoint = values[4 + (pointsCount - 2) * 3];
					ImVec2 lastTangent = values[3 + (pointsCount - 2) * 3];

					m_BezierCubicPoints.emplace_back(lastTangent.x, lastTangent.y);
					m_BezierCubicPoints.emplace_back(lastPoint.x, lastPoint.y);

					m_Emitter.VelocityCurvePoints[pointIndex] = glm::vec4(lastTangent.x, lastTangent.y, lastPoint.x, lastPoint.y);

					m_Emitter.VelocityCurvePointCount = m_BezierCubicPoints.size();

				/*	for (int i = 0; i < m_BezierCubicPoints.size(); i++)
					{
						CR_LOG_DEBUG("{0}: X: {1}, Y: {2}", i, m_BezierCubicPoints[i].x, m_BezierCubicPoints[i].y);
					}
					CR_LOG_DEBUG("===========================");
					for (int i = 0; i < m_BezierCubicPoints.size() / 2; i++)
					{
						CR_LOG_DEBUG("{0}: X: {1}, Y: {2}", i, m_Emitter.VelocityCurvePoints[i].x, m_Emitter.VelocityCurvePoints[i].y);
						CR_LOG_DEBUG("{0}: X: {1}, Y: {2}", i, m_Emitter.VelocityCurvePoints[i].z, m_Emitter.VelocityCurvePoints[i].w);
					} */
				}
			}

			if (ImGui::CollapsingHeader("Debug"))
			{
				ImGui::Checkbox("Enable Sorting", &m_EnableSorting);

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
					ScopedMap<Particle, StorageBuffer> buffer(m_ParticleBuffers.ParticleBuffer);
					CR_LOG_DEBUG("Dumped Particle Buffer:");
					for (int i = 0; i < m_MaxParticles; i++)
					{
						CR_LOG_DEBUG("[{0}] {1} ({2})", i, buffer[i].Position.y, buffer[i].Color.r);
					}
					CR_LOG_DEBUG("===========================================\n");
				}

				if (ImGui::Button("Dump Counter Buffer"))
				{
					ScopedMap<CounterBuffer, StorageBuffer> buffer(m_ParticleBuffers.CounterBuffer);
					CR_LOG_DEBUG("Dumped Counter Buffer:");
					CR_LOG_DEBUG("AliveCount {0}", buffer->AliveCount);
					CR_LOG_DEBUG("DeadCount {0}", buffer->DeadCount);
					CR_LOG_DEBUG("RealEmitCount {0}", buffer->RealEmitCount);
					CR_LOG_DEBUG("AliveCount_AfterSimulation {0}", buffer->AliveCount_AfterSimulation);
					CR_LOG_DEBUG("===========================================\n");
				}

				if (ImGui::Button("Dump Draw Index Buffer"))
				{
					ScopedMap<uint32_t, StorageBuffer> buffer(m_ParticleBuffers.DrawParticleIndexBuffer);
					CR_LOG_DEBUG("Dumped Draw Particle Index Buffer:");
					for (int i = 0; i < m_MaxParticles; i++)
					{
						CR_LOG_DEBUG("[{0}] {1}", i, buffer[i]);
					}
					CR_LOG_DEBUG("===========================================\n");
				}


				if (ImGui::Button("Dump AlivePreSimulate Buffer"))
				{
					ScopedMap<uint32_t, StorageBuffer> buffer(m_ParticleBuffers.AliveBufferPreSimulate);
					CR_LOG_DEBUG("Dumped AlivePreSimulate Buffer:");
					for (int i = 0; i < m_MaxParticles; i += 4)
					{
						CR_LOG_DEBUG("{0} {1} {2} {3}", buffer[i], buffer[i + 1], buffer[i + 2], buffer[i + 3]);
					}
					CR_LOG_DEBUG("===========================================\n");
				}

				if (ImGui::Button("Dump AlivePostSimulate Buffer"))
				{
					CR_LOG_DEBUG("Dumped AlivePostSimulate Buffer:");
					ScopedMap<uint32_t, StorageBuffer> buffer(m_ParticleBuffers.AliveBufferPostSimulate);
					for (int i = 0; i < m_MaxParticles; i += 4)
					{
						CR_LOG_DEBUG("{0} {1} {2} {3}", buffer[i], buffer[i + 1], buffer[i + 2], buffer[i + 3]);
					}
					CR_LOG_DEBUG("===========================================\n");
				}

				if (ImGui::Button("Dump Camera Distance Buffer"))
				{
					CR_LOG_DEBUG("Dumped Camera Distance Buffer:");
					ScopedMap<uint32_t, StorageBuffer> buffer(m_ParticleBuffers.CameraDistanceBuffer);
					for (int i = 0; i < m_MaxParticles; i++)
					{
						CR_LOG_DEBUG("{0}", buffer[i]);
					}
					CR_LOG_DEBUG("===========================================\n");
				}
			}

			ImGui::End();
		}

	}

	

	int CurveEditor(const char* label
		, float* values
		, int points_count
		, const ImVec2& editor_size
		, ImU32 flags
		, int* new_count)
	{
		enum class StorageValues : ImGuiID
		{
			FROM_X = 100,
			FROM_Y,
			WIDTH,
			HEIGHT,
			IS_PANNING,
			POINT_START_X,
			POINT_START_Y
		};

		const float HEIGHT = 100;
		static ImVec2 start_pan;

		ImGuiContext& g = *GImGui;
		const ImGuiStyle& style = g.Style;
		ImVec2 size = editor_size;
		size.x = size.x < 0 ? CalcItemWidth() + (style.FramePadding.x * 2) : size.x;
		size.y = size.y < 0 ? HEIGHT : size.y;

		ImGuiWindow* parent_window = GetCurrentWindow();
		ImGuiID id = parent_window->GetID(label);
		if (!BeginChildFrame(id, size, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
		{
			EndChild();
			return -1;
		}

		int hovered_idx = -1;
		if (new_count) *new_count = points_count;

		ImGuiWindow* window = GetCurrentWindow();
		if (window->SkipItems)
		{
			EndChild();
			return -1;
		}

		ImVec2 points_min(FLT_MAX, FLT_MAX);
		ImVec2 points_max(-FLT_MAX, -FLT_MAX);
		for (int point_idx = 0; point_idx < points_count; ++point_idx)
		{
			ImVec2 point;
			if (flags & (int)CurveEditorFlags::NO_TANGENTS)
			{
				point = ((ImVec2*)values)[point_idx];
			}
			else
			{
				point = ((ImVec2*)values)[1 + point_idx * 3];
			}
			points_max = ImMax(points_max, point);
			points_min = ImMin(points_min, point);
		}
		points_max.y = ImMax(points_max.y, points_min.y + 0.0001f);

		float from_x = window->StateStorage.GetFloat((ImGuiID)StorageValues::FROM_X, points_min.x);
		float from_y = window->StateStorage.GetFloat((ImGuiID)StorageValues::FROM_Y, points_min.y);
		float width = window->StateStorage.GetFloat((ImGuiID)StorageValues::WIDTH, points_max.x - points_min.x);
		float height = window->StateStorage.GetFloat((ImGuiID)StorageValues::HEIGHT, points_max.y - points_min.y);
		window->StateStorage.SetFloat((ImGuiID)StorageValues::FROM_X, from_x);
		window->StateStorage.SetFloat((ImGuiID)StorageValues::FROM_Y, from_y);
		window->StateStorage.SetFloat((ImGuiID)StorageValues::WIDTH, width);
		window->StateStorage.SetFloat((ImGuiID)StorageValues::HEIGHT, height);

		ImVec2 beg_pos = GetCursorScreenPos();

		const ImRect inner_bb = window->InnerRect;
		const ImRect frame_bb(inner_bb.Min - style.FramePadding, inner_bb.Max + style.FramePadding);

		auto transform = [&](const ImVec2& pos) -> ImVec2
		{
			float x = (pos.x - from_x) / width;
			float y = (pos.y - from_y) / height;

			return ImVec2(
				inner_bb.Min.x * (1 - x) + inner_bb.Max.x * x,
				inner_bb.Min.y * y + inner_bb.Max.y * (1 - y)
			);
		};

		auto invTransform = [&](const ImVec2& pos) -> ImVec2
		{
			float x = (pos.x - inner_bb.Min.x) / (inner_bb.Max.x - inner_bb.Min.x);
			float y = (inner_bb.Max.y - pos.y) / (inner_bb.Max.y - inner_bb.Min.y);

			return ImVec2(
				from_x + width * x,
				from_y + height * y
			);
		};

		if (flags & (int)CurveEditorFlags::SHOW_GRID)
		{
			int exp;
			frexp(width / 5, &exp);
			float step_x = (float)ldexp(1.0, exp);
			int cell_cols = int(width / step_x);

			float x = step_x * int(from_x / step_x);
			for (int i = -1; i < cell_cols + 2; ++i)
			{
				ImVec2 a = transform({ x + i * step_x, from_y });
				ImVec2 b = transform({ x + i * step_x, from_y + height });
				window->DrawList->AddLine(a, b, 0x55000000);
				char buf[64];
				if (exp > 0)
				{
					ImFormatString(buf, sizeof(buf), " %d", int(x + i * step_x));
				}
				else
				{
					ImFormatString(buf, sizeof(buf), " %0.2f", x + i * step_x);
				}
				window->DrawList->AddText(b, 0x55000000, buf);
			}

			frexp(height / 5, &exp);
			float step_y = (float)ldexp(1.0, exp);
			int cell_rows = int(height / step_y);

			float y = step_y * int(from_y / step_y);
			for (int i = -1; i < cell_rows + 2; ++i)
			{
				ImVec2 a = transform({ from_x, y + i * step_y });
				ImVec2 b = transform({ from_x + width, y + i * step_y });
				window->DrawList->AddLine(a, b, 0x55000000);
				char buf[64];
				if (exp > 0)
				{
					ImFormatString(buf, sizeof(buf), " %d", int(y + i * step_y));
				}
				else
				{
					ImFormatString(buf, sizeof(buf), " %0.2f", y + i * step_y);
				}
				window->DrawList->AddText(a, 0x55000000, buf);
			}
		}

		if (ImGui::GetIO().MouseWheel != 0 && ImGui::IsItemHovered())
		{
			float scale = powf(2, ImGui::GetIO().MouseWheel);
			width *= scale;
			height *= scale;
			window->StateStorage.SetFloat((ImGuiID)StorageValues::WIDTH, width);
			window->StateStorage.SetFloat((ImGuiID)StorageValues::HEIGHT, height);
		}
		if (ImGui::IsMouseReleased(1))
		{
			window->StateStorage.SetBool((ImGuiID)StorageValues::IS_PANNING, false);
		}
		if (window->StateStorage.GetBool((ImGuiID)StorageValues::IS_PANNING, false))
		{
			ImVec2 drag_offset = ImGui::GetMouseDragDelta(1);
			from_x = start_pan.x;
			from_y = start_pan.y;
			from_x -= drag_offset.x * width / (inner_bb.Max.x - inner_bb.Min.x);
			from_y += drag_offset.y * height / (inner_bb.Max.y - inner_bb.Min.y);
			window->StateStorage.SetFloat((ImGuiID)StorageValues::FROM_X, from_x);
			window->StateStorage.SetFloat((ImGuiID)StorageValues::FROM_Y, from_y);
		}
		else if (ImGui::IsMouseDragging(1) && ImGui::IsWindowHovered())
		{
			window->StateStorage.SetBool((ImGuiID)StorageValues::IS_PANNING, true);
			start_pan.x = from_x;
			start_pan.y = from_y;
		}

		int changed_idx = -1;
		for (int point_idx = points_count - 2; point_idx >= 0; --point_idx)
		{
			ImVec2* points;
			if (flags & (int)CurveEditorFlags::NO_TANGENTS)
			{
				points = ((ImVec2*)values) + point_idx;
			}
			else
			{
				points = ((ImVec2*)values) + 1 + point_idx * 3;
			}

			ImVec2 p_prev = points[0];
			ImVec2 p_next = points[6];
			ImVec2 tangent_last;
			ImVec2 tangent;
			ImVec2 p;

			if (flags & (int)CurveEditorFlags::NO_TANGENTS)
			{
				p = points[1];
			}
			else
			{
				tangent_last = points[1];
				tangent = points[2];
				p = points[3];
			}

			auto handlePoint = [&](ImVec2& p, int idx) -> bool
			{
				static const float SIZE = 4;

				ImVec2 cursor_pos = GetCursorScreenPos();
				ImVec2 pos = transform(p);

				SetCursorScreenPos(pos - ImVec2(SIZE, SIZE));
				PushID(idx + 100);
				InvisibleButton("", ImVec2(2 * NODE_SLOT_RADIUS, 2 * NODE_SLOT_RADIUS));

				ImU32 col = IsItemActive() || IsItemHovered() ? GetColorU32(ImGuiCol_PlotLinesHovered) : GetColorU32(ImGuiCol_PlotLines);

				window->DrawList->AddCircle(pos, SIZE, col);

				if (IsItemHovered()) hovered_idx = point_idx + idx;

				bool changed = false;
				if (IsItemActive() && IsMouseClicked(0))
				{
					window->StateStorage.SetFloat((ImGuiID)StorageValues::POINT_START_X, pos.x);
					window->StateStorage.SetFloat((ImGuiID)StorageValues::POINT_START_Y, pos.y);
				}

				if (IsItemHovered() || IsItemActive() && IsMouseDragging(0))
				{
					char tmp[64];
					ImFormatString(tmp, sizeof(tmp), "%0.2f, %0.2f", p.x, p.y);
					window->DrawList->AddText({ pos.x, pos.y - GetTextLineHeight() }, 0xff000000, tmp);
				}

				if (IsItemActive() && IsMouseDragging(0))
				{
					pos.x = window->StateStorage.GetFloat((ImGuiID)StorageValues::POINT_START_X, pos.x);
					pos.y = window->StateStorage.GetFloat((ImGuiID)StorageValues::POINT_START_Y, pos.y);
					pos += ImGui::GetMouseDragDelta();
					ImVec2 v = invTransform(pos);

					p = v;
					changed = true;
				}
				PopID();

				SetCursorScreenPos(cursor_pos);
				return changed;
			};

			auto handleTangent = [&](ImVec2& t, const ImVec2& p, int idx) -> bool
			{
				static const float SIZE = 2;

				auto normalized = [](const ImVec2& v) -> ImVec2
				{
					float len = 1.0f / sqrtf(v.x * v.x + v.y * v.y);
					return ImVec2(v.x * len, v.y * len);
				};

				const float PIXEL_SPACE = 50.0f;

				ImVec2 cursor_pos = GetCursorScreenPos();
				ImVec2 pos = transform(p);
				ImVec2 tang = pos + ImVec2(t.x, -t.y) * PIXEL_SPACE;

				SetCursorScreenPos(tang - ImVec2(SIZE, SIZE));
				PushID(-idx);
				InvisibleButton("", ImVec2(2 * NODE_SLOT_RADIUS, 2 * NODE_SLOT_RADIUS));

				window->DrawList->AddLine(pos, tang, GetColorU32(ImGuiCol_PlotLines));

				ImU32 col = IsItemHovered() ? GetColorU32(ImGuiCol_PlotLinesHovered) : GetColorU32(ImGuiCol_PlotLines);

				window->DrawList->AddRect(tang + ImVec2(SIZE, SIZE), tang + ImVec2(-SIZE, -SIZE), col);

				bool changed = false;
				if (IsItemActive() && IsMouseDragging(0))
				{
					tang = GetIO().MousePos - pos;
					tang.y *= -1;

					t = tang / PIXEL_SPACE;
					changed = true;
				}
				PopID();

				SetCursorScreenPos(cursor_pos);
				return changed;
			};

			const float LENGTH = 1.0f;

			float distance = std::min(abs(p.x - p_next.x), abs(p.x - p_prev.x));

			PushID(point_idx);
			if ((flags & (int)CurveEditorFlags::NO_TANGENTS) == 0)
			{
				window->DrawList->AddBezierCurve(
					transform(p_prev), // points[0]
					transform(p_prev + tangent_last * distance / LENGTH), // points[0] + points[1]
					transform(p + tangent * distance / LENGTH), // points[3] + points[2]
					transform(p), // points[3]
					GetColorU32(ImGuiCol_PlotLines),
					1.0f,
					20);

				if (handleTangent(tangent_last, p_prev, 0))
				{
					points[1] = ImClamp(tangent_last, ImVec2(0.0f, -LENGTH), ImVec2(LENGTH, LENGTH));
					changed_idx = point_idx;
				}
				if (handleTangent(tangent, p, 1))
				{
					points[2] = ImClamp(tangent, ImVec2(-LENGTH, -LENGTH), ImVec2(0.0f, LENGTH));
					changed_idx = point_idx + 1;
				}
				if (handlePoint(p, 1))
				{
					if (p.x <= p_prev.x) p.x = p_prev.x + 0.001f;
					if (point_idx < points_count - 2 && p.x >= points[6].x)
					{
						p.x = points[6].x - 0.001f;
					}
					points[3] = p;
					changed_idx = point_idx + 1;
				}
			}
			else
			{
				window->DrawList->AddLine(transform(p_prev), transform(p), GetColorU32(ImGuiCol_PlotLines), 1.0f);
				if (handlePoint(p, 1))
				{
					if (p.x <= p_prev.x) p.x = p_prev.x + 0.001f;
					if (point_idx < points_count - 2 && p.x >= points[2].x)
					{
						p.x = points[2].x - 0.001f;
					}
					points[1] = p;
					changed_idx = point_idx + 1;
				}
			}
			if (point_idx == 0)
			{
				if (handlePoint(p_prev, 0))
				{
					if (p.x <= p_prev.x) p_prev.x = p.x - 0.001f;
					points[0] = p_prev;
					changed_idx = point_idx;
				}
			}

			// Debug point
			float t = s_GraphTime;
			glm::vec2 point = BezierCubicCalc(
				*(glm::vec2*)&transform(p_prev), // points[0]
				*(glm::vec2*)&transform(p_prev + tangent_last * distance / LENGTH), // points[0] + points[1]
				*(glm::vec2*)&transform(p + tangent * distance / LENGTH), // points[3] + points[2]
				*(glm::vec2*)&transform(p),
				t);

			window->DrawList->AddCircleFilled(ImVec2(point.x, point.y), 5.0f, 0xffff00ff);

			PopID();
		}

		SetCursorScreenPos(inner_bb.Min);

		InvisibleButton("bg", inner_bb.Max - inner_bb.Min);

		if (ImGui::IsItemActive() && ImGui::IsMouseDoubleClicked(0) && new_count)
		{
			ImVec2 mp = ImGui::GetMousePos();
			ImVec2 new_p = invTransform(mp);
			ImVec2* points = (ImVec2*)values;

			if ((flags & (int)CurveEditorFlags::NO_TANGENTS) == 0)
			{
				const float DEFAULT_TANGENT = 0.4f;

				points[points_count * 3 + 0] = ImVec2(-DEFAULT_TANGENT, 0);
				points[points_count * 3 + 1] = new_p;
				points[points_count * 3 + 2] = ImVec2(DEFAULT_TANGENT, 0);
				++* new_count;

				auto compare = [](const void* a, const void* b) -> int
				{
					float fa = (((const ImVec2*)a) + 1)->x;
					float fb = (((const ImVec2*)b) + 1)->x;
					return fa < fb ? -1 : (fa > fb) ? 1 : 0;
				};

				qsort(values, points_count + 1, sizeof(ImVec2) * 3, compare);
			}
			else
			{
				points[points_count] = new_p;
				++* new_count;

				auto compare = [](const void* a, const void* b) -> int
				{
					float fa = ((const ImVec2*)a)->x;
					float fb = ((const ImVec2*)b)->x;
					return fa < fb ? -1 : (fa > fb) ? 1 : 0;
				};

				qsort(values, points_count + 1, sizeof(ImVec2), compare);
			}
		}

		if (hovered_idx >= 0 && ImGui::IsMouseDoubleClicked(0) && new_count && points_count > 2)
		{
			ImVec2* points = (ImVec2*)values;
			--* new_count;
			if ((flags & (int)CurveEditorFlags::NO_TANGENTS) == 0)
			{
				for (int j = hovered_idx * 3; j < points_count * 3 - 3; j += 3)
				{
					points[j + 0] = points[j + 3];
					points[j + 1] = points[j + 4];
					points[j + 2] = points[j + 5];
				}
			}
			else
			{
				for (int j = hovered_idx; j < points_count - 1; ++j)
				{
					points[j] = points[j + 1];
				}
			}
		}

		EndChildFrame();
		RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, inner_bb.Min.y), label);
		return changed_idx;
	}
	
}