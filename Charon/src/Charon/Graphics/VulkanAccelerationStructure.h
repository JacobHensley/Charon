#pragma once

#include "Mesh.h"

#include "VulkanAllocator.h"
#include "vulkan/vulkan.h"

namespace Charon {

	struct AccelerationStructureSpecification
	{
		Ref<Mesh> Mesh;
		glm::mat4 Transform;
	};

	class VulkanAccelerationStructure
	{
	public:
		struct VulkanAccelerationStructureInfo
		{
			VkAccelerationStructureKHR AccelerationStructure = nullptr;
			VkDeviceAddress DeviceAddress = 0;
			VkBuffer ASBuffer = nullptr;
			VmaAllocation ASMemory = nullptr;
			VkBuffer ScratchBuffer = nullptr;
			VmaAllocation ScratchMemory = nullptr;
			VkBuffer InstancesBuffer = nullptr;
			VmaAllocation InstancesMemory = nullptr;
			VkBuffer InstancesUploadBuffer = nullptr;
			VmaAllocation InstancesUploadMemory = nullptr;
		};
	public:
		VulkanAccelerationStructure(const AccelerationStructureSpecification& specification);
		~VulkanAccelerationStructure();

		const VkAccelerationStructureKHR& GetAccelerationStructure() { return m_TopLevelAccelerationStructure.AccelerationStructure; }
	private:
		void Init();

		void CreateTopLevelAccelerationStructure();
		void CreateBottomLevelAccelerationStructure(Ref<Mesh> mesh, const SubMesh& submesh, VulkanAccelerationStructureInfo& outInfo);
	private:
		AccelerationStructureSpecification m_Specification;
		VulkanAccelerationStructureInfo m_TopLevelAccelerationStructure;
		std::vector<VulkanAccelerationStructureInfo> m_BottomLevelAccelerationStructure;
	};

}