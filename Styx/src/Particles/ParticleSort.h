#pragma once

#include "Charon/Core/Layer.h"
#include "Charon/Core/Core.h"
#include "Charon/Core/Application.h"
#include "Charon/Graphics/Camera.h"
#include "Charon/Graphics/VulkanComputePipeline.h"
#include "Charon/Graphics/VulkanPipeline.h"
#include "Charon/Graphics/Buffers.h"
#include "Charon/Graphics/Renderer.h"

namespace Charon {

	class ParticleSort
	{
		struct SortPipeline
		{
			Ref<Shader> Shader;
			Ref<VulkanComputePipeline> Pipeline;
		};
	public:
		ParticleSort(uint32_t maxParticles);

		void Init();
	private:
		void CompileAndCreatePipeline(SortPipeline& pipeline, std::string_view entryPoint, const std::string& define = "");
		void CreateBuffers();
		void CreateWriteDescriptor(VkWriteDescriptorSet& set, Ref<StorageBuffer> buffer, uint32_t binding, VkDescriptorType type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		VkBufferMemoryBarrier BufferTransition(VkBuffer buffer, VkAccessFlags before, VkAccessFlags after, uint32_t size);
		void BindConstantBuffer(VkDescriptorBufferInfo& GPUCB, VkDescriptorSet& DescriptorSet, uint32_t Binding = 0, uint32_t Count = 1);
		void Sort(VkCommandBuffer commandList, bool isBenchmarking, float benchmarkTime);
	private:
		Ref<Shader> m_ParallelSortShader;

		uint32_t m_MaxParticles = 0;

		Ref<StorageBuffer> m_IndirectKeyCounts;
		Ref<StorageBuffer> m_IndirectCountScatterArgs, m_IndirectReduceScanArgs, m_IndirectConstantBuffer;

		// Radix sort scratch buffers
		uint32_t m_ScratchBufferSize, m_ReducedScratchBufferSize;
		Ref<StorageBuffer> m_FPSScratchBuffer, m_FPSReducedScratchBuffer;

		struct ComputePipelines
		{
			SortPipeline FPS_SetupIndirectParameters;
			SortPipeline FPS_Count;
			SortPipeline FPS_CountReduce;
			SortPipeline FPS_Scan;
			SortPipeline FPS_ScanAdd;
			SortPipeline FPS_Scatter;
			SortPipeline FPS_ScatterPayload; // (#define kRS_ValueCopy 1)
		} m_ComputePipelines;

		struct WriteDescriptor
		{
			std::vector<VkWriteDescriptorSet> SortKeyPayload; // set 2
			std::vector<VkWriteDescriptorSet> ScanData[2]; // set 3
			std::vector<VkWriteDescriptorSet> SumTable; // set 4
			std::vector<VkWriteDescriptorSet> IndirectBuffers; // set 5
		} m_WriteDescriptors;
	};

}