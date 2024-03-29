#include "pch.h"
#include "RayTracingLayer.h"
#include "Charon/Core/Core.h"
#include "Charon/Core/Application.h"
#include "Charon/Graphics/Mesh.h"
#include "Charon/Graphics/SceneRenderer.h"
#include "Charon/Scene/Components.h"
#include "Charon/ImGui/imgui_impl_vulkan_with_textures.h"
#include "imgui/imgui.h"

#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>

namespace Charon {

	namespace Utils {

		inline VkWriteDescriptorSet WriteDescriptorSet(
			VkDescriptorSet dstSet,
			VkDescriptorType type,
			uint32_t binding,
			const VkDescriptorBufferInfo* bufferInfo,
			uint32_t descriptorCount = 1)
		{
			VkWriteDescriptorSet writeDescriptorSet{};
			writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSet.dstSet = dstSet;
			writeDescriptorSet.descriptorType = type;
			writeDescriptorSet.dstBinding = binding;
			writeDescriptorSet.pBufferInfo = bufferInfo;
			writeDescriptorSet.descriptorCount = descriptorCount;
			return writeDescriptorSet;
		}

		inline VkWriteDescriptorSet WriteDescriptorSet(
			VkDescriptorSet dstSet,
			VkDescriptorType type,
			uint32_t binding,
			const VkDescriptorImageInfo* imageInfo,
			uint32_t descriptorCount = 1)
		{
			VkWriteDescriptorSet writeDescriptorSet{};
			writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSet.dstSet = dstSet;
			writeDescriptorSet.descriptorType = type;
			writeDescriptorSet.dstBinding = binding;
			writeDescriptorSet.pImageInfo = imageInfo;
			writeDescriptorSet.descriptorCount = descriptorCount;
			return writeDescriptorSet;
		}
	}


	RayTracingLayer::RayTracingLayer()
		: Layer("RayTracing")
	{
		Init();
	}

	void RayTracingLayer::Init()
	{
		m_Scene = CreateRef<Scene>();
		m_Camera = CreateRef<Camera>(glm::perspectiveFov(glm::radians(45.0f), 1280.0f, 720.0f, 0.1f, 100.0f));
		m_ViewportPanel = CreateRef<ViewportPanel>();

		m_SceneObject = m_Scene->CreateObject("Test Object");
		//m_MeshHandle = AssetManager::Load<Mesh>("assets/models/CornellWithSphere.gltf");
		m_MeshHandle = AssetManager::Load<Mesh>("assets/models/new-sponza/Sponza-Baked.gltf");

		{
			AccelerationStructureSpecification spec;
			spec.Mesh = AssetManager::Get<Mesh>(m_MeshHandle);
			spec.Transform = glm::mat4(1.0f);
			m_AccelerationStructure = CreateRef<VulkanAccelerationStructure>(spec);
		}

		if (!CreateRayTracingPipeline())
			CR_LOG_CRITICAL("Failed to create Ray Tracing pipeline!");

		{
			ImageSpecification spec;
			spec.Format = VK_FORMAT_R8G8B8A8_UNORM;
			spec.Usage = VK_IMAGE_USAGE_STORAGE_BIT;
			spec.Width = 1280;
			spec.Height = 720;
			m_Image = CreateRef<Image>(spec);
		} 
		{
			ImageSpecification spec;
			spec.Format = VK_FORMAT_R32G32B32A32_SFLOAT;
			spec.Usage = VK_IMAGE_USAGE_STORAGE_BIT;
			spec.Width = 1280;
			spec.Height = 720;
			m_AccumulationImage = CreateRef<Image>(spec);
		}

		m_SceneBuffer.DirectionalLight_Direction = glm::normalize(glm::vec3(-0.66f, -0.577f, -0.577f));
		m_SceneBuffer.PointLight_Position = glm::vec3(-5);
		m_SceneBuffer.FrameIndex = 1;
		m_SceneUB = CreateRef<UniformBuffer>(&m_SceneBuffer, sizeof(SceneBuffer));

		m_SceneObject.AddComponent<MeshComponent>(m_MeshHandle);
	}

	void RayTracingLayer::OnUpdate()
	{
		m_Camera->Update();
		m_Scene->Update();

		if (!m_Accumulate)
			m_SceneBuffer.FrameIndex = 1;
		m_SceneUB->UpdateBuffer(&m_SceneBuffer);
	}

	void RayTracingLayer::RayTracingPass()
	{
		if (m_RTWidth == 0 || m_RTHeight == 0)
			return;

		// Resize image if needed
		if (m_Image->GetSpecification().Width != m_RTWidth || m_Image->GetSpecification().Height != m_RTHeight)
		{
			m_Image->Resize(m_RTWidth, m_RTHeight);
			m_AccumulationImage->Resize(m_RTWidth, m_RTHeight);
			m_Camera->SetProjectionMatrix(glm::perspectiveFov(glm::radians(45.0f), (float)m_RTWidth, (float)m_RTHeight, 0.1f, 100.0f));
		}

		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();
		auto renderer = Application::GetApp().GetRenderer();
		VkCommandBuffer commandBuffer = renderer->GetActiveCommandBuffer();

		Ref<UniformBuffer> uniformBuffer = renderer->GetCameraUB();
		Ref<StorageBuffer> submeshDataSB = m_AccelerationStructure->GetSubmeshDataStorageBuffer();

		VkDescriptorSetAllocateInfo descriptorSetInfo{};
		descriptorSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetInfo.pSetLayouts = &m_RayTracingPipeline->GetDescriptorSetLayout();
		descriptorSetInfo.descriptorSetCount = 1;

		VkDescriptorSet descriptorSet = renderer->AllocateDescriptorSet(descriptorSetInfo);

		VkWriteDescriptorSetAccelerationStructureKHR asDescriptorWrite{};
		asDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
		asDescriptorWrite.accelerationStructureCount = 1;
		asDescriptorWrite.pAccelerationStructures = &m_AccelerationStructure->GetAccelerationStructure();

		VkWriteDescriptorSet accelerationStructureWrite{};
		accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		accelerationStructureWrite.pNext = &asDescriptorWrite;
		accelerationStructureWrite.dstSet = descriptorSet;
		accelerationStructureWrite.dstBinding = 0;
		accelerationStructureWrite.descriptorCount = 1;
		accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

		const auto& asSpec = m_AccelerationStructure->GetSpecification();

		std::vector<VkDescriptorBufferInfo> vertexBufferInfos;
		{
			VkBuffer vb = asSpec.Mesh->GetVertexBuffer()->GetBuffer();
			vertexBufferInfos.push_back({ vb, 0, VK_WHOLE_SIZE });
		}

		std::vector<VkDescriptorBufferInfo> indexBufferInfos;
		{
			VkBuffer ib = asSpec.Mesh->GetIndexBuffer()->GetBuffer();
			indexBufferInfos.push_back({ ib, 0, VK_WHOLE_SIZE });
		}
		
		std::vector<VkDescriptorImageInfo> textureImageInfos;
		{
			const auto& textures = m_AccelerationStructure->GetTextures();
			for (auto texture : textures)
			{
				textureImageInfos.push_back(texture->GetDescriptorImageInfo());
			}
		}

		std::vector<VkWriteDescriptorSet> rayTracingWriteDescriptors = {
			accelerationStructureWrite,
			Utils::WriteDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &m_Image->GetDescriptorImageInfo()),
			Utils::WriteDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2, &m_AccumulationImage->GetDescriptorImageInfo()),
			Utils::WriteDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3, &uniformBuffer->getDescriptorBufferInfo()),
			Utils::WriteDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4, vertexBufferInfos.data(), (uint32_t)vertexBufferInfos.size()),
			Utils::WriteDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5, indexBufferInfos.data(), (uint32_t)indexBufferInfos.size()),
			Utils::WriteDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6, &submeshDataSB->getDescriptorBufferInfo()),
			Utils::WriteDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 7, &m_SceneUB->getDescriptorBufferInfo()),
			Utils::WriteDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8, &m_AccelerationStructure->GetMaterialBuffer()->getDescriptorBufferInfo()),
			Utils::WriteDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 9, textureImageInfos.data(), (uint32_t)textureImageInfos.size()),
		};

		vkUpdateDescriptorSets(device->GetLogicalDevice(), rayTracingWriteDescriptors.size(), rayTracingWriteDescriptors.data(), 0, NULL);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_RayTracingPipeline->GetPipeline());
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_RayTracingPipeline->GetPipelineLayout(), 0, 1, &descriptorSet, 0, 0);

		const auto& shaderBindingTable = m_RayTracingPipeline->GetShaderBindingTable();

		VkStridedDeviceAddressRegionKHR empty{};

		vkCmdTraceRaysKHR(commandBuffer,
			&shaderBindingTable[0].StridedDeviceAddressRegion,
			&shaderBindingTable[1].StridedDeviceAddressRegion,
			&shaderBindingTable[2].StridedDeviceAddressRegion,
			&empty,
			m_RTWidth,
			m_RTHeight,
			1);

		m_SceneBuffer.FrameIndex++;
	}

	bool RayTracingLayer::CreateRayTracingPipeline()
	{
		RayTracingPipelineSpecification spec;
		spec.RayGenShader = CreateRef<Shader>("assets/shaders/RayTracing/RayGen.glsl");
		if (!spec.RayGenShader->CompiledSuccessfully())
			return false;

		spec.MissShader = CreateRef<Shader>("assets/shaders/RayTracing/Miss.glsl");
		if (!spec.MissShader->CompiledSuccessfully())
			return false;

		spec.ClosestHitShader = CreateRef<Shader>("assets/shaders/RayTracing/ClosestHit.glsl");
		if (!spec.ClosestHitShader->CompiledSuccessfully())
			return false;

		m_RayTracingPipeline = CreateRef<VulkanRayTracingPipeline>(spec);
		return true;
	}

	void RayTracingLayer::OnRender()
	{
		RayTracingPass();

		m_Scene->Render(m_Camera);
	}

	void RayTracingLayer::OnImGUIRender()
	{
		m_ViewportPanel->Render(nullptr);

		ImGui::Begin("Ray Tracing");
		const auto& descriptorInfo = m_Image->GetDescriptorImageInfo();
		m_RTWidth = (uint32_t)ImGui::GetContentRegionAvail().x;
		m_RTHeight = (uint32_t)ImGui::GetContentRegionAvail().y;

		ImTextureID imTex = ImGui_ImplVulkan_AddTexture(descriptorInfo.sampler, descriptorInfo.imageView, descriptorInfo.imageLayout);
		ImGui::Image(imTex, { (float)m_RTWidth, (float)m_RTHeight}, ImVec2(0, 1), ImVec2(1, 0));
		ImGui::End();

		if (ImGui::Begin("Settings"))
		{
			if (ImGui::Button("Reload Pipeline"))
			{
				if (!CreateRayTracingPipeline())
					CR_LOG_CRITICAL("Failed to create Ray Tracing pipeline!");
			}

			if (ImGui::Button("Render"))
			{
				m_SceneBuffer.FrameIndex = 1;
			}

			ImGui::Checkbox("Accumulate", &m_Accumulate);

			ImGui::DragFloat3("Directional Light", glm::value_ptr(m_SceneBuffer.DirectionalLight_Direction), 0.01f, -1.0f, 1.0f);
			ImGui::DragFloat3("Point Light", glm::value_ptr(m_SceneBuffer.PointLight_Position), 0.01f);
		}
		ImGui::End();

#if 0
		ImGui::Begin("Viewport");

		auto& descriptorInfo = SceneRenderer::GetFinalBuffer()->GetImage(0)->GetDescriptorImageInfo();
		ImTextureID imTex = ImGui_ImplVulkan_AddTexture(descriptorInfo.sampler, descriptorInfo.imageView, descriptorInfo.imageLayout);

		const auto& fbSpec = SceneRenderer::GetFinalBuffer()->GetSpecification();
		float width = ImGui::GetContentRegionAvail().x;
		float aspect = (float)fbSpec.Height / (float)fbSpec.Width;

		ImGui::Image(imTex, { width, width * aspect }, ImVec2(0, 1), ImVec2(1, 0));

		ImGui::End();

		ImGui::ShowDemoWindow();
#endif
	}

	

}