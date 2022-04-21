#pragma once
#include "VulkanAllocator.h"
#include <vulkan/vulkan.h>

namespace Charon {

	struct BufferInfo
	{
		VkBuffer Buffer = nullptr;
		VmaAllocation Allocation = nullptr;
	};

	template<typename T, typename BufferType>
	class ScopedMap
	{
	public:
		ScopedMap(std::shared_ptr<BufferType> buffer)
			: m_Buffer(buffer)
		{
			m_MappedData = m_Buffer->Map<T>();
		}

		~ScopedMap()
		{
			m_Buffer->Unmap();
		}

		T& operator*()
		{
			return *m_MappedData;
		}

		const T& operator*() const
		{
			return *m_MappedData;
		}

		T* operator->()
		{
			return m_MappedData;
		}

		T& operator[](size_t index)
		{
			return m_MappedData[index];
		}

		const T* operator->() const
		{
			return m_MappedData;
		}

	private:
		std::shared_ptr<BufferType> m_Buffer;
		T* m_MappedData;
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
		IndexBuffer(uint32_t size, uint32_t count);
		~IndexBuffer();

	public:
		VkBuffer GetBuffer() { return m_BufferInfo.Buffer; }
		uint32_t GetCount() { return m_Count; }

		template<typename T>
		T* Map()
		{
			VulkanAllocator allocator("IndexBuffer");
			return allocator.MapMemory<T>(m_BufferInfo.Allocation);
		}

		void Unmap()
		{
			VulkanAllocator allocator("IndexBuffer");
			return allocator.UnmapMemory(m_BufferInfo.Allocation);
		}

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

		template<typename T>
		T* Map()
		{
			VulkanAllocator allocator("UniformBuffer");
			return (T*)allocator.MapMemory<T>(m_BufferInfo.Allocation);
		}

		void Unmap()
		{
			VulkanAllocator allocator("UniformBuffer");
			return allocator.UnmapMemory(m_BufferInfo.Allocation);
		}

	private:
		BufferInfo m_BufferInfo;
		VkDescriptorBufferInfo m_DescriptorBufferInfo;
		uint32_t m_Size = 0;
	};

	// Storage Buffer
	class StorageBuffer
	{
	public:
		StorageBuffer(uint32_t size, bool cpu = false, bool vertex = false);
		StorageBuffer(uint32_t size, VkBufferUsageFlagBits usageFlags);
		~StorageBuffer();

	public:
		VkBuffer GetBuffer() { return m_BufferInfo.Buffer; }
		BufferInfo GetBufferInfo() { return m_BufferInfo; }
		const VkDescriptorBufferInfo& getDescriptorBufferInfo() { return m_DescriptorBufferInfo; }

		template<typename T>
		T* Map()
		{
			VulkanAllocator allocator("StorageBuffer");
			return allocator.MapMemory<T>(m_BufferInfo.Allocation);
		}

		void Unmap()
		{
			VulkanAllocator allocator("StorageBuffer");
			return allocator.UnmapMemory(m_BufferInfo.Allocation);
		}

		uint32_t GetSize() const { return m_Size; }
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