#include "pch.h"
#include "Buffers.h"

namespace Charon {

	VertexBuffer::VertexBuffer(void* vertexData, uint32_t size)
	{
		// Create buffer info
		VkBufferCreateInfo vertexBufferCreateInfo = {};
		vertexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertexBufferCreateInfo.size = size;
		vertexBufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
		vertexBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		// Allocate memory
		VulkanAllocator allocator("VertexBuffer");
		m_BufferInfo.Allocation = allocator.AllocateBuffer(vertexBufferCreateInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, m_BufferInfo.Buffer);

		// Copy data into buffer
		void* dstBuffer = allocator.MapMemory<void>(m_BufferInfo.Allocation);
		memcpy(dstBuffer, vertexData, size);
		allocator.UnmapMemory(m_BufferInfo.Allocation);
	}

	VertexBuffer::~VertexBuffer()
	{
		VulkanAllocator allocator("VertexBuffer");
		allocator.DestroyBuffer(m_BufferInfo.Buffer, m_BufferInfo.Allocation);
	}

	IndexBuffer::IndexBuffer(void* indexData, uint32_t size, uint32_t count)
		: m_Count(count) 
	{
		// Create buffer info
		VkBufferCreateInfo vertexBufferCreateInfo = {};
		vertexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertexBufferCreateInfo.size = size;
		vertexBufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

		// Allocate memory
		VulkanAllocator allocator("IndexBuffer");
		m_BufferInfo.Allocation = allocator.AllocateBuffer(vertexBufferCreateInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, m_BufferInfo.Buffer);

		// Copy data into buffer
		void* dstBuffer = allocator.MapMemory<void>(m_BufferInfo.Allocation);
		memcpy(dstBuffer, indexData, size);
		allocator.UnmapMemory(m_BufferInfo.Allocation);
	}

	IndexBuffer::IndexBuffer(uint32_t size, uint32_t count)
		: m_Count(count)
	{
		// Create buffer info
		VkBufferCreateInfo vertexBufferCreateInfo = {};
		vertexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertexBufferCreateInfo.size = size;
		vertexBufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

		// Allocate memory
		VulkanAllocator allocator("IndexBuffer");
		m_BufferInfo.Allocation = allocator.AllocateBuffer(vertexBufferCreateInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, m_BufferInfo.Buffer);
	}

	IndexBuffer::~IndexBuffer()
	{
		VulkanAllocator allocator("IndexBuffer");
		allocator.DestroyBuffer(m_BufferInfo.Buffer, m_BufferInfo.Allocation);
	}

	UniformBuffer::UniformBuffer(void* data, uint32_t size)
		: m_Size(size)
	{
		// Create buffer info
		VkBufferCreateInfo uniformBufferCreateInfo = {};
		uniformBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		uniformBufferCreateInfo.size = size;
		uniformBufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		// Allocate memory
		VulkanAllocator allocator("UniformBuffer");
		m_BufferInfo.Allocation = allocator.AllocateBuffer(uniformBufferCreateInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, m_BufferInfo.Buffer);

		if (data)
			UpdateBuffer(data);

		m_DescriptorBufferInfo.buffer = m_BufferInfo.Buffer;
		m_DescriptorBufferInfo.offset = 0;
		m_DescriptorBufferInfo.range = size;
	}

	UniformBuffer::~UniformBuffer()
	{
		VulkanAllocator allocator("UniformBuffer");
		allocator.DestroyBuffer(m_BufferInfo.Buffer, m_BufferInfo.Allocation);
	}

	void UniformBuffer::UpdateBuffer(void* data)
	{
		// Copy data into buffer
		VulkanAllocator allocator("UniformBuffer");
		void* dstBuffer = allocator.MapMemory<void>(m_BufferInfo.Allocation);
		memcpy(dstBuffer, data, m_Size);
		allocator.UnmapMemory(m_BufferInfo.Allocation);
	}

	StorageBuffer::StorageBuffer(uint32_t size, bool cpu, bool vertex)
		: m_Size(size)
	{
		// Create buffer info
		VkBufferCreateInfo vertexBufferCreateInfo = {};
		vertexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertexBufferCreateInfo.size = size;
		vertexBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		if (vertex)
			vertexBufferCreateInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

		// Allocate memory
		VulkanAllocator allocator("StorageBuffer");
		m_BufferInfo.Allocation = allocator.AllocateBuffer(vertexBufferCreateInfo, cpu ? VMA_MEMORY_USAGE_CPU_ONLY : VMA_MEMORY_USAGE_CPU_TO_GPU, m_BufferInfo.Buffer);

		m_DescriptorBufferInfo.buffer = m_BufferInfo.Buffer;
		m_DescriptorBufferInfo.offset = 0;
		m_DescriptorBufferInfo.range = size;
	}

	StorageBuffer::StorageBuffer(uint32_t size, VkBufferUsageFlagBits usageFlags)
		: m_Size(size)
	{
		// Create buffer info
		VkBufferCreateInfo vertexBufferCreateInfo = {};
		vertexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertexBufferCreateInfo.size = size;
		vertexBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | usageFlags;

		// Allocate memory
		VulkanAllocator allocator("StorageBuffer");
		m_BufferInfo.Allocation = allocator.AllocateBuffer(vertexBufferCreateInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, m_BufferInfo.Buffer);

		m_DescriptorBufferInfo.buffer = m_BufferInfo.Buffer;
		m_DescriptorBufferInfo.offset = 0;
		m_DescriptorBufferInfo.range = size;
	}

	StorageBuffer::~StorageBuffer()
	{
		VulkanAllocator allocator("StorageBuffer");
		allocator.DestroyBuffer(m_BufferInfo.Buffer, m_BufferInfo.Allocation);
	}

	VulkanBuffer::VulkanBuffer(void* data, uint32_t size, VkBufferUsageFlagBits bufferType, VmaMemoryUsage memoryType)
	{
		// Create buffer info
		VkBufferCreateInfo vertexBufferCreateInfo = {};
		vertexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertexBufferCreateInfo.size = size;
		vertexBufferCreateInfo.usage = bufferType;

		// Allocate memory
		VulkanAllocator allocator("VulkanBuffer");
		m_BufferInfo.Allocation = allocator.AllocateBuffer(vertexBufferCreateInfo, memoryType, m_BufferInfo.Buffer);

		// Copy data into buffer
		if (data != nullptr)
		{
			void* dstBuffer = allocator.MapMemory<void>(m_BufferInfo.Allocation);
			memcpy(dstBuffer, data, size);
			allocator.UnmapMemory(m_BufferInfo.Allocation);
		}
	}

	VulkanBuffer::~VulkanBuffer()
	{
		VulkanAllocator allocator("VulkanBuffer");
		allocator.DestroyBuffer(m_BufferInfo.Buffer, m_BufferInfo.Allocation);
	}


}