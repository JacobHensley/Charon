#pragma once
#include "VulkanAllocator.h"
#include <vulkan/vulkan.h>

namespace Charon {

	struct BufferInfo
	{
		VkBuffer Buffer = nullptr;
		VmaAllocation Allocation = nullptr;
	};

	// Vertex Buffer
	class VertexBuffer
	{
	public:
		VertexBuffer(void* vertexData, uint32_t size);
		~VertexBuffer();

	public:
		VkBuffer GetBuffer() { return m_BufferInfo.Buffer; }

	private:
		BufferInfo m_BufferInfo;
	};

	// Index Buffer
	class IndexBuffer
	{
	public:
		IndexBuffer(void* indexData, uint32_t size, uint32_t count);
		~IndexBuffer();

	public:
		VkBuffer GetBuffer() { return m_BufferInfo.Buffer; }
		uint32_t GetCount() { return m_Count; }

	private:
		BufferInfo m_BufferInfo;
		uint32_t m_Count = 0;
	};

	// Uniform Buffer
	class UniformBuffer
	{
	public:
		UniformBuffer(void* data, uint32_t size);
		~UniformBuffer();

	public:
		VkBuffer GetBuffer() { return m_BufferInfo.Buffer; }

		void UpdateBuffer(void* data);
		const VkDescriptorBufferInfo& getDescriptorBufferInfo() { return m_DescriptorBufferInfo; }

	private:
		BufferInfo m_BufferInfo;
		VkDescriptorBufferInfo m_DescriptorBufferInfo;
		uint32_t m_Size = 0;
	};

	// Storage Buffer
	class StorageBuffer
	{
	public:
		StorageBuffer(uint32_t size);
		~StorageBuffer();

	public:
		VkBuffer GetBuffer() { return m_BufferInfo.Buffer; }
		const VkDescriptorBufferInfo& getDescriptorBufferInfo() { return m_DescriptorBufferInfo; }

	private:
		BufferInfo m_BufferInfo;
		VkDescriptorBufferInfo m_DescriptorBufferInfo;
		uint32_t m_Size = 0;
	};

	// Vulkan Buffer
	class VulkanBuffer
	{
	public:
		VulkanBuffer(void* data, uint32_t size, VkBufferUsageFlagBits bufferType, VmaMemoryUsage memoryType);
		~VulkanBuffer();

	public:
		VkBuffer GetVulkanBuffer() { return m_BufferInfo.Buffer; }

	private:
		BufferInfo m_BufferInfo;
	};

}