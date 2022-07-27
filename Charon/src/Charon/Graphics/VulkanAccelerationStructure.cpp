#include "pch.h"
#include "VulkanAccelerationStructure.h"

#include "Charon/Core/Application.h"

namespace Charon {


	VulkanAccelerationStructure::VulkanAccelerationStructure(const AccelerationStructureSpecification& specification)
		: m_Specification(specification)
	{
		Init();
	}

	VulkanAccelerationStructure::~VulkanAccelerationStructure()
	{

	}

	void VulkanAccelerationStructure::Init()
	{
		if (m_Specification.Mesh)
		{
			const auto& submeshes = m_Specification.Mesh->GetSubMeshes();
			m_BottomLevelAccelerationStructure.resize(submeshes.size());
			for (size_t i = 0; i < submeshes.size(); i++)
			{
				auto& info = m_BottomLevelAccelerationStructure[i];
				CreateBottomLevelAccelerationStructure(m_Specification.Mesh, submeshes[i], info);
			}
		}

	}

	void VulkanAccelerationStructure::CreateBottomLevelAccelerationStructure(Ref<Mesh> mesh, const SubMesh& submesh, VulkanAccelerationStructureInfo& outInfo)
	{
		VkDevice device = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();
		VulkanAllocator allocator("AccelerationStructure");

		const auto& vertexBuffer = mesh->GetVertexBuffer();
		const auto& indexBuffer = mesh->GetIndexBuffer();

		uint32_t primitiveCount = submesh.IndexCount / 3;

		VkAccelerationStructureGeometryTrianglesDataKHR trianglesData{};
		trianglesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
		trianglesData.vertexData = 0; // device buffer stuff
		trianglesData.vertexStride = sizeof(Vertex);
		trianglesData.maxVertex = submesh.VertexCount;
		trianglesData.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		trianglesData.indexData = 0; // device buffer stuff
		trianglesData.indexType = VK_INDEX_TYPE_UINT16;

		VkAccelerationStructureGeometryDataKHR geometryData{};
		geometryData.triangles = trianglesData;
		
		VkAccelerationStructureGeometryKHR geometry{};
		geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		geometry.geometry = geometryData;
		geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

		VkBuildAccelerationStructureFlagBitsKHR buildFlags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;

		VkAccelerationStructureBuildGeometryInfoKHR inputs{};
		inputs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		inputs.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		inputs.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		inputs.geometryCount = 1;
		inputs.pGeometries = &geometry;
		inputs.flags = buildFlags;

		VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
		sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &inputs, &primitiveCount, &sizeInfo);

		VkBufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;

		// ASBuffer
		bufferCreateInfo.size = sizeInfo.accelerationStructureSize;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		outInfo.ASMemory = allocator.AllocateBuffer(bufferCreateInfo, VMA_MEMORY_USAGE_GPU_ONLY, outInfo.ASBuffer);

		// ScratchBuffer
		bufferCreateInfo.size = sizeInfo.buildScratchSize;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; // TODO: do we really need VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		outInfo.ScratchMemory = allocator.AllocateBuffer(bufferCreateInfo, VMA_MEMORY_USAGE_GPU_ONLY, outInfo.ScratchBuffer);
		inputs.scratchData = 0; // outInfo.ScratchBuffer device buffer stuff

		VkAccelerationStructureCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		createInfo.size = sizeInfo.accelerationStructureSize;
		createInfo.buffer = outInfo.ASBuffer;

		vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &outInfo.AccelerationStructure);

		inputs.dstAccelerationStructure = outInfo.AccelerationStructure;

		std::vector<VkAccelerationStructureBuildRangeInfoKHR*> buildRangeInfos(1);
		VkAccelerationStructureBuildRangeInfoKHR buildInfo = { primitiveCount, 0, 0, 0 }; // TODO: offsets here?
		buildRangeInfos[0] = &buildInfo;

		VkCommandBuffer commandBuffer = Application::GetApp().GetVulkanDevice()->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &inputs, buildRangeInfos.data());

		VkMemoryBarrier barrier;
		barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			0, 1, &barrier, 0, nullptr, 0, nullptr);

		Application::GetApp().GetVulkanDevice()->FlushCommandBuffer(commandBuffer, true);
	}

}