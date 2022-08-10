#include "pch.h"
#include "VulkanAccelerationStructure.h"

#include "Charon/Core/Application.h"

#include <glm/gtc/type_ptr.hpp>

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

			CreateTopLevelAccelerationStructure();
		}

	}

	void VulkanAccelerationStructure::CreateTopLevelAccelerationStructure()
	{
		VkDevice device = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();
		VulkanAllocator allocator("AccelerationStructure");

		std::vector<VkAccelerationStructureInstanceKHR> instances;

		// Doing this for "every model" even though we only have one. Also blah blah somthing about instancing the objects for no dupes but we're not doing that yet.
		const auto& submeshes = m_Specification.Mesh->GetSubMeshes();
		for (size_t i = 0; i < submeshes.size(); i++)
		{
			const SubMesh& submesh = submeshes[i];

			glm::mat4 rmWorldTransform = glm::transpose(m_Specification.Transform * submesh.Transform); // Row-major

			VkAccelerationStructureDeviceAddressInfoKHR asDeviceAddressInfo = {};
			asDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
			asDeviceAddressInfo.accelerationStructure = m_BottomLevelAccelerationStructure[i].AccelerationStructure;
			VkDeviceAddress blasAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &asDeviceAddressInfo);

			VkAccelerationStructureInstanceKHR& accelerationAtructureInstance = instances.emplace_back();
			memcpy(accelerationAtructureInstance.transform.matrix, glm::value_ptr(rmWorldTransform), sizeof(VkTransformMatrixKHR));
			accelerationAtructureInstance.instanceCustomIndex = i; // TODO: come back to this
			accelerationAtructureInstance.mask = 0xFF;
			accelerationAtructureInstance.instanceShaderBindingTableRecordOffset = 0;
			accelerationAtructureInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT_KHR;
			accelerationAtructureInstance.accelerationStructureReference = blasAddress;
		}

		// TODO: use staging buffer
		VkBufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = sizeof(VkAccelerationStructureInstanceKHR) * instances.size();
		bufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		m_TopLevelAccelerationStructure.InstancesMemory = allocator.AllocateBuffer(bufferCreateInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, m_TopLevelAccelerationStructure.InstancesBuffer);

		void* dstBuffer = allocator.MapMemory<void>(m_TopLevelAccelerationStructure.InstancesMemory);
		memcpy(dstBuffer, instances.data(), bufferCreateInfo.size);
		allocator.UnmapMemory(m_TopLevelAccelerationStructure.InstancesMemory);

		VkDeviceOrHostAddressConstKHR instance_data_device_address{};
		instance_data_device_address.deviceAddress = GetVulkanDeviceAddress(m_TopLevelAccelerationStructure.InstancesBuffer);

		VkAccelerationStructureGeometryDataKHR geometryData{};
		geometryData.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		geometryData.instances.arrayOfPointers = VK_FALSE;
		geometryData.instances.data = instance_data_device_address;

		VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
		accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		accelerationStructureGeometry.geometry = geometryData;

		VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
		accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
		accelerationStructureBuildGeometryInfo.geometryCount = 1;
		accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

		uint32_t primitive_count = instances.size();

		VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
		accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &accelerationStructureBuildGeometryInfo, &primitive_count, &accelerationStructureBuildSizesInfo);
		
		bufferCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		m_TopLevelAccelerationStructure.ASMemory = allocator.AllocateBuffer(bufferCreateInfo, VMA_MEMORY_USAGE_GPU_ONLY, m_TopLevelAccelerationStructure.ASBuffer);

		VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
		accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		accelerationStructureCreateInfo.buffer = m_TopLevelAccelerationStructure.ASBuffer;
		accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
		accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		vkCreateAccelerationStructureKHR(device, &accelerationStructureCreateInfo, nullptr, &m_TopLevelAccelerationStructure.AccelerationStructure);

		bufferCreateInfo.size = accelerationStructureBuildSizesInfo.buildScratchSize;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		m_TopLevelAccelerationStructure.ScratchMemory = allocator.AllocateBuffer(bufferCreateInfo, VMA_MEMORY_USAGE_GPU_ONLY, m_TopLevelAccelerationStructure.ScratchBuffer);

		VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
		accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
		accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		accelerationBuildGeometryInfo.dstAccelerationStructure = m_TopLevelAccelerationStructure.AccelerationStructure;
		accelerationBuildGeometryInfo.geometryCount = 1;
		accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
		accelerationBuildGeometryInfo.scratchData.deviceAddress = GetVulkanDeviceAddress(m_TopLevelAccelerationStructure.ScratchBuffer);

		VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo;
		accelerationStructureBuildRangeInfo.primitiveCount = primitive_count;
		accelerationStructureBuildRangeInfo.primitiveOffset = 0;
		accelerationStructureBuildRangeInfo.firstVertex = 0;
		accelerationStructureBuildRangeInfo.transformOffset = 0;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

		VkCommandBuffer commandBuffer = Application::GetApp().GetVulkanDevice()->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &accelerationBuildGeometryInfo, accelerationBuildStructureRangeInfos.data());

		VkMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			0, 1, &barrier, 0, nullptr, 0, nullptr);

		Application::GetApp().GetVulkanDevice()->FlushCommandBuffer(commandBuffer, true);

		VkAccelerationStructureDeviceAddressInfoKHR acceleration_device_address_info{};
		acceleration_device_address_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		acceleration_device_address_info.accelerationStructure = m_TopLevelAccelerationStructure.AccelerationStructure;
		m_TopLevelAccelerationStructure.DeviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &acceleration_device_address_info);
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
		trianglesData.vertexData.deviceAddress = GetVulkanDeviceAddress(mesh->GetVertexBuffer()->GetBuffer()); // Modified
		trianglesData.vertexStride = sizeof(Vertex);
		trianglesData.maxVertex = submesh.VertexCount;
		trianglesData.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		trianglesData.indexData.deviceAddress = GetVulkanDeviceAddress(mesh->GetIndexBuffer()->GetBuffer()); // Modified
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
		bufferCreateInfo.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT; // TODO: do we really need VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		outInfo.ScratchMemory = allocator.AllocateBuffer(bufferCreateInfo, VMA_MEMORY_USAGE_GPU_ONLY, outInfo.ScratchBuffer);
		inputs.scratchData.deviceAddress = GetVulkanDeviceAddress(outInfo.ScratchBuffer); // Modified

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

		VkMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			0, 1, &barrier, 0, nullptr, 0, nullptr);

		Application::GetApp().GetVulkanDevice()->FlushCommandBuffer(commandBuffer, true);
	}

	uint64_t VulkanAccelerationStructure::GetVulkanDeviceAddress(VkBuffer handle)
	{
		VkDevice device = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();
		VkBufferDeviceAddressInfoKHR buffer_device_address_info{};
		buffer_device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		buffer_device_address_info.buffer = handle;
		return vkGetBufferDeviceAddressKHR(device, &buffer_device_address_info);
	}

}