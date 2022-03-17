#include "ParticleSort.h"
#define FFX_CPP
#include "../assets/shaders/sorting/FFX_ParallelSort.h"

namespace Charon {

	ParticleSort::ParticleSort(uint32_t maxParticles)
		: m_MaxParticles(maxParticles)
	{
	}

	void ParticleSort::Init()
	{
		// Load shader
		m_ParallelSortShader = CreateRef<Shader>("assets/shaders/sorting/ParallelSortCS.hlsl", "FPS_SetupIndirectParameters");

		CreateBuffers();
		CompileAndCreatePipeline(m_ComputePipelines.FPS_SetupIndirectParameters, "FPS_SetupIndirectParameters");
		CompileAndCreatePipeline(m_ComputePipelines.FPS_Count, "FPS_Count");
		CompileAndCreatePipeline(m_ComputePipelines.FPS_CountReduce, "FPS_CountReduce");
		CompileAndCreatePipeline(m_ComputePipelines.FPS_Scan, "FPS_Scan");
		CompileAndCreatePipeline(m_ComputePipelines.FPS_ScanAdd, "FPS_ScanAdd");
		CompileAndCreatePipeline(m_ComputePipelines.FPS_Scatter, "FPS_Scatter");

		CompileAndCreatePipeline(m_ComputePipelines.FPS_ScatterPayload, "FPS_Scatter", "kRS_ValueCopy"); //#define kRS_ValueCopy
		
		__debugbreak();
	}

	void ParticleSort::CompileAndCreatePipeline(SortPipeline& pipeline, std::string_view entryPoint, const std::string& define)
	{
        if (!define.empty())
        {
            std::vector<std::wstring> defines = { std::wstring(define.begin(), define.end()) };
            pipeline.Shader = CreateRef<Shader>("assets/shaders/sorting/ParallelSortCS.hlsl", entryPoint, defines);
        }

		pipeline.Shader = CreateRef<Shader>("assets/shaders/sorting/ParallelSortCS.hlsl", entryPoint);
		pipeline.Pipeline = CreateRef<VulkanComputePipeline>(pipeline.Shader);
	}

	void ParticleSort::CreateBuffers()
	{
		uint32_t bufferUsage = VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		m_IndirectKeyCounts = CreateRef<StorageBuffer>(sizeof(uint32_t) * 3, bufferUsage);

		// Create scratch buffers for radix sort
		FFX_ParallelSort_CalculateScratchResourceSize(m_MaxParticles, m_ScratchBufferSize, m_ReducedScratchBufferSize);
		m_FPSScratchBuffer = CreateRef<StorageBuffer>(m_ScratchBufferSize, bufferUsage);
		m_FPSReducedScratchBuffer = CreateRef<StorageBuffer>(m_ReducedScratchBufferSize, bufferUsage);

		bufferUsage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		m_IndirectCountScatterArgs = CreateRef<StorageBuffer>(sizeof(uint32_t) * 3, bufferUsage);
		m_IndirectReduceScanArgs = CreateRef<StorageBuffer>(sizeof(uint32_t) * 3, bufferUsage);

		bufferUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		m_IndirectConstantBuffer = CreateRef<StorageBuffer>(sizeof(FFX_ParallelSortCB), bufferUsage);

		// Create write descriptors
		
		// ScanData -> set 3 (variant 0)
		CreateWriteDescriptor(m_WriteDescriptors.ScanData[0].emplace_back(), m_FPSReducedScratchBuffer, 0);
		CreateWriteDescriptor(m_WriteDescriptors.ScanData[0].emplace_back(), m_FPSReducedScratchBuffer, 1);
		// ScanData -> set 3 (variant 1)
		CreateWriteDescriptor(m_WriteDescriptors.ScanData[1].emplace_back(), m_FPSScratchBuffer, 0);
		CreateWriteDescriptor(m_WriteDescriptors.ScanData[1].emplace_back(), m_FPSScratchBuffer, 1);
		CreateWriteDescriptor(m_WriteDescriptors.ScanData[1].emplace_back(), m_FPSReducedScratchBuffer, 2);
		// ScanData -> set 4
		CreateWriteDescriptor(m_WriteDescriptors.SumTable.emplace_back(), m_FPSScratchBuffer, 0);
		CreateWriteDescriptor(m_WriteDescriptors.SumTable.emplace_back(), m_FPSReducedScratchBuffer, 1);
		// ScanData -> set 5
		CreateWriteDescriptor(m_WriteDescriptors.IndirectBuffers.emplace_back(), m_IndirectKeyCounts, 0);
		CreateWriteDescriptor(m_WriteDescriptors.IndirectBuffers.emplace_back(), m_IndirectConstantBuffer, 1);
		CreateWriteDescriptor(m_WriteDescriptors.IndirectBuffers.emplace_back(), m_IndirectCountScatterArgs, 2);
		CreateWriteDescriptor(m_WriteDescriptors.IndirectBuffers.emplace_back(), m_IndirectReduceScanArgs, 3);

		// TODO: vkUpdateDescriptorSets (preferably once)
	}

	void ParticleSort::CreateWriteDescriptor(VkWriteDescriptorSet& set, Ref<StorageBuffer> buffer, uint32_t binding, VkDescriptorType type)
	{
		set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		set.descriptorType = type;
		set.dstBinding = 0;
		set.pBufferInfo = &buffer->getDescriptorBufferInfo();
		set.descriptorCount = 1;
	}

	VkBufferMemoryBarrier ParticleSort::BufferTransition(VkBuffer buffer, VkAccessFlags before, VkAccessFlags after, uint32_t size)
	{
		VkBufferMemoryBarrier bufferBarrier = {};
		bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		bufferBarrier.srcAccessMask = before;
		bufferBarrier.dstAccessMask = after;
		bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufferBarrier.buffer = buffer;
		bufferBarrier.size = size;

		return bufferBarrier;
	}

    void ParticleSort::BindConstantBuffer(VkDescriptorBufferInfo& GPUCB, VkDescriptorSet& DescriptorSet, uint32_t Binding/*=0*/, uint32_t Count/*=1*/)
    {
        Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();

        VkWriteDescriptorSet write_set = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        write_set.pNext = nullptr;
        write_set.dstSet = DescriptorSet;
        write_set.dstBinding = Binding;
        write_set.dstArrayElement = 0;
        write_set.descriptorCount = Count;
        write_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write_set.pImageInfo = nullptr;
        write_set.pBufferInfo = &GPUCB;
        write_set.pTexelBufferView = nullptr;
        vkUpdateDescriptorSets(device->GetLogicalDevice(), 1, &write_set, 0, nullptr);
    }

    // Perform Parallel Sort (radix-based sort)
    void ParticleSort::Sort(VkCommandBuffer commandList, bool isBenchmarking, float benchmarkTime)
    {
        bool bIndirectDispatch = m_UIIndirectSort;

        // To control which descriptor set to use for updating data
        static uint32_t frameCount = 0;
        uint32_t frameConstants = (++frameCount) % 3;

        std::string markerText = "FFXParallelSort";
        if (bIndirectDispatch) markerText += " Indirect";
     // SetPerfMarkerBegin(commandList, markerText.c_str());

        // Buffers to ping-pong between when writing out sorted values
        VkBuffer* ReadBufferInfo(&m_DstKeyBuffers[0]), * WriteBufferInfo(&m_DstKeyBuffers[1]);
        VkBuffer* ReadPayloadBufferInfo(&m_DstPayloadBuffers[0]), * WritePayloadBufferInfo(&m_DstPayloadBuffers[1]);
        bool bHasPayload = m_UISortPayload;

        // Setup barriers for the run
        VkBufferMemoryBarrier Barriers[3];
        FFX_ParallelSortCB  constantBufferData = { 0 };

        // Fill in the constant buffer data structure (this will be done by a shader in the indirect version)
        uint32_t NumThreadgroupsToRun;
        uint32_t NumReducedThreadgroupsToRun;
        if (!bIndirectDispatch)
        {
            uint32_t NumberOfKeys = NumKeys[m_UIResolutionSize];
            FFX_ParallelSort_SetConstantAndDispatchData(NumberOfKeys, m_MaxNumThreadgroups, constantBufferData, NumThreadgroupsToRun, NumReducedThreadgroupsToRun);
        }
        else
        {
            struct SetupIndirectCB
            {
                uint32_t NumKeysIndex;
                uint32_t MaxThreadGroups;
            };
            SetupIndirectCB IndirectSetupCB;
            IndirectSetupCB.NumKeysIndex = m_UIResolutionSize;
            IndirectSetupCB.MaxThreadGroups = m_MaxNumThreadgroups;

            // Copy the data into the constant buffer
            VkDescriptorBufferInfo constantBuffer = m_pConstantBufferRing->AllocConstantBuffer(sizeof(SetupIndirectCB), (void*)&IndirectSetupCB);
            BindConstantBuffer(constantBuffer, m_SortDescriptorSetConstantsIndirect[frameConstants]);

            // Dispatch
            vkCmdBindDescriptorSets(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_SortPipelineLayout, 1, 1, &m_SortDescriptorSetConstantsIndirect[frameConstants], 0, nullptr);
            vkCmdBindDescriptorSets(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_SortPipelineLayout, 5, 1, &m_SortDescriptorSetIndirect, 0, nullptr);
            vkCmdBindPipeline(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_ComputePipelines.FPS_SetupIndirectParameters.Pipeline->GetPipeline());
            vkCmdDispatch(commandList, 1, 1, 1);

            // When done, transition the args buffers to INDIRECT_ARGUMENT, and the constant buffer UAV to Constant buffer
            VkBufferMemoryBarrier barriers[5];
            barriers[0] = BufferTransition(m_IndirectCountScatterArgs->GetBuffer(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, sizeof(uint32_t) * 3);
            barriers[1] = BufferTransition(m_IndirectReduceScanArgs->GetBuffer(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, sizeof(uint32_t) * 3);
            barriers[2] = BufferTransition(m_IndirectConstantBuffer->GetBuffer(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, sizeof(FFX_ParallelSortCB));
            barriers[3] = BufferTransition(m_IndirectCountScatterArgs->GetBuffer(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT, sizeof(uint32_t) * 3);
            barriers[4] = BufferTransition(m_IndirectReduceScanArgs->GetBuffer(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT, sizeof(uint32_t) * 3);
            vkCmdPipelineBarrier(commandList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 5, barriers, 0, nullptr);
        }

        // Bind the scratch descriptor sets
        vkCmdBindDescriptorSets(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_SortPipelineLayout, 4, 1, &m_SortDescriptorSetScratch, 0, nullptr);

        // Copy the data into the constant buffer and bind
        if (bIndirectDispatch)
        {
            //constantBuffer = m_IndirectConstantBuffer.GetResource()->GetGPUVirtualAddress();
            VkDescriptorBufferInfo constantBuffer;
            constantBuffer.buffer = m_IndirectConstantBuffer->GetBuffer();
            constantBuffer.offset = 0;
            constantBuffer.range = VK_WHOLE_SIZE;
            BindConstantBuffer(constantBuffer, m_SortDescriptorSetConstants[frameConstants]);
        }
        else
        {
            VkDescriptorBufferInfo constantBuffer = m_pConstantBufferRing->AllocConstantBuffer(sizeof(FFX_ParallelSortCB), (void*)&constantBufferData);
            BindConstantBuffer(constantBuffer, m_SortDescriptorSetConstants[frameConstants]);
        }
        // Bind constants
        vkCmdBindDescriptorSets(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_SortPipelineLayout, 0, 1, &m_SortDescriptorSetConstants[frameConstants], 0, nullptr);

        // Perform Radix Sort (currently only support 32-bit key/payload sorting
        uint32_t inputSet = 0;
        for (uint32_t Shift = 0; Shift < 32u; Shift += FFX_PARALLELSORT_SORT_BITS_PER_PASS)
        {
            // Update the bit shift
            vkCmdPushConstants(commandList, m_SortPipelineLayout, VK_SHADER_STAGE_ALL, 0, 4, &Shift);

            // Bind input/output for this pass
            vkCmdBindDescriptorSets(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_SortPipelineLayout, 2, 1, &m_SortDescriptorSetInputOutput[inputSet], 0, nullptr);

            // Sort Count
            {
                vkCmdBindPipeline(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_ComputePipelines.FPS_Count.Pipeline->GetPipeline());

                if (bIndirectDispatch)
                    vkCmdDispatchIndirect(commandList, m_IndirectCountScatterArgs->GetBuffer() 0);
                else
                    vkCmdDispatch(commandList, NumThreadgroupsToRun, 1, 1);
            }

            // UAV barrier on the sum table
            Barriers[0] = BufferTransition(m_FPSScratchBuffer->GetBuffer(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, m_ScratchBufferSize);
            vkCmdPipelineBarrier(commandList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, Barriers, 0, nullptr);

            // Sort Reduce
            {
                vkCmdBindPipeline(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_ComputePipelines.FPS_CountReduce.Pipeline->GetPipeline());

                if (bIndirectDispatch)
                    vkCmdDispatchIndirect(commandList, m_IndirectReduceScanArgs->GetBuffer(), 0);
                else
                    vkCmdDispatch(commandList, NumReducedThreadgroupsToRun, 1, 1);

                // UAV barrier on the reduced sum table
                Barriers[0] = BufferTransition(m_FPSReducedScratchBuffer->GetBuffer(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, m_ReducedScratchBufferSize);
                vkCmdPipelineBarrier(commandList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, Barriers, 0, nullptr);
            }

            // Sort Scan
            {
                // First do scan prefix of reduced values
                vkCmdBindDescriptorSets(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_SortPipelineLayout, 3, 1, &m_SortDescriptorSetScanSets[0], 0, nullptr);
                vkCmdBindPipeline(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_ComputePipelines.FPS_Scan.Pipeline->GetPipeline());

                if (!bIndirectDispatch)
                {
                    assert(NumReducedThreadgroupsToRun < FFX_PARALLELSORT_ELEMENTS_PER_THREAD* FFX_PARALLELSORT_THREADGROUP_SIZE && "Need to account for bigger reduced histogram scan");
                }
                vkCmdDispatch(commandList, 1, 1, 1);

                // UAV barrier on the reduced sum table
                Barriers[0] = BufferTransition(m_FPSReducedScratchBuffer->GetBuffer(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, m_ReducedScratchBufferSize);
                vkCmdPipelineBarrier(commandList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, Barriers, 0, nullptr);

                // Next do scan prefix on the histogram with partial sums that we just did
                vkCmdBindDescriptorSets(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_SortPipelineLayout, 3, 1, &m_SortDescriptorSetScanSets[1], 0, nullptr);

                vkCmdBindPipeline(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_ComputePipelines.FPS_ScanAdd.Pipeline->GetPipeline());
                if (bIndirectDispatch)
                    vkCmdDispatchIndirect(commandList, m_IndirectReduceScanArgs->GetBuffer(), 0);
                else
                    vkCmdDispatch(commandList, NumReducedThreadgroupsToRun, 1, 1);
            }

            // UAV barrier on the sum table
            Barriers[0] = BufferTransition(m_FPSScratchBuffer->GetBuffer(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, m_ScratchBufferSize);
            vkCmdPipelineBarrier(commandList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, Barriers, 0, nullptr);

            // Sort Scatter
            {
                vkCmdBindPipeline(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, bHasPayload ? m_ComputePipelines.FPS_ScatterPayload.Pipeline->GetPipeline() : m_ComputePipelines.FPS_Scatter.Pipeline->GetPipeline());

                if (bIndirectDispatch)
                    vkCmdDispatchIndirect(commandList, m_IndirectCountScatterArgs->GetBuffer(), 0);
                else
                    vkCmdDispatch(commandList, NumThreadgroupsToRun, 1, 1);
            }

            // Finish doing everything and barrier for the next pass
            int numBarriers = 0;
            Barriers[numBarriers++] = BufferTransition(*WriteBufferInfo, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, sizeof(uint32_t) * NumKeys[2]);
            if (bHasPayload)
                Barriers[numBarriers++] = BufferTransition(*WritePayloadBufferInfo, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, sizeof(uint32_t) * NumKeys[2]);
            vkCmdPipelineBarrier(commandList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, numBarriers, Barriers, 0, nullptr);

            // Swap read/write sources
            std::swap(ReadBufferInfo, WriteBufferInfo);
            if (bHasPayload)
                std::swap(ReadPayloadBufferInfo, WritePayloadBufferInfo);
            inputSet = !inputSet;
        }

        // When we are all done, transition indirect buffers back to UAV for the next frame (if doing indirect dispatch)
        if (bIndirectDispatch)
        {
            VkBufferMemoryBarrier barriers[3];
            barriers[0] = BufferTransition(m_IndirectConstantBuffer->GetBuffer(), VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, sizeof(FFX_ParallelSortCB));
            barriers[1] = BufferTransition(m_IndirectCountScatterArgs->GetBuffer(), VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, sizeof(uint32_t) * 3);
            barriers[2] = BufferTransition(m_IndirectReduceScanArgs->GetBuffer(), VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, sizeof(uint32_t) * 3);
            vkCmdPipelineBarrier(commandList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 3, barriers, 0, nullptr);
        }

        // Close out the perf capture
        // SetPerfMarkerEnd(commandList);
    }

}