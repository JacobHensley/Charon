#include "ParticleSort.h"
#define FFX_CPP
#include "../assets/shaders/sorting/FFX_ParallelSort.h"

namespace Charon {

	ParticleSort::ParticleSort()
	{
	}

    void ParticleSort::Init(uint32_t maxParticles)
    {
        m_MaxParticles = maxParticles;

        Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();

        // Create buffers
        CreateBuffers();

        // Compile shaders
        CompileShader(m_ComputePipelines.FPS_Count, "FPS_Count");
        CompileShader(m_ComputePipelines.FPS_CountReduce, "FPS_CountReduce");
        CompileShader(m_ComputePipelines.FPS_Scan, "FPS_Scan");
        CompileShader(m_ComputePipelines.FPS_ScanAdd, "FPS_ScanAdd");
        CompileShader(m_ComputePipelines.FPS_Scatter, "FPS_Scatter");
        CompileShader(m_ComputePipelines.FPS_ScatterPayload, "FPS_Scatter", "kRS_ValueCopy"); //#define kRS_ValueCopy

        // Create main sorting pipeline
        {
            VkPushConstantRange constant_range;
            constant_range.stageFlags = VK_SHADER_STAGE_ALL;
            constant_range.offset = 0;
            constant_range.size = 4;

            VkPipelineLayoutCreateInfo layout_create_info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
            layout_create_info.pNext = nullptr;
            layout_create_info.flags = 0;
            layout_create_info.setLayoutCount = 6;

            std::vector<VkDescriptorSetLayout> layouts(layout_create_info.setLayoutCount);
            layouts[0] = m_ComputePipelines.FPS_Count.Shader->GetDescriptorSetLayout(0);
            layouts[2] = m_ComputePipelines.FPS_ScatterPayload.Shader->GetDescriptorSetLayout(2);
            layouts[3] = m_ComputePipelines.FPS_ScanAdd.Shader->GetDescriptorSetLayout(3);
            layouts[4] = m_ComputePipelines.FPS_CountReduce.Shader->GetDescriptorSetLayout(4);

            for (size_t i = 0; i < layouts.size(); i++)
            {
                if (!layouts[i])
                {
                    VkDescriptorSetLayoutCreateInfo layoutInfo{};
                    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                    layoutInfo.bindingCount = 0;
                    layoutInfo.pBindings = nullptr;
                    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->GetLogicalDevice(), &layoutInfo, nullptr, &layouts[i]));
                }
            }

            layout_create_info.pSetLayouts = layouts.data();
            layout_create_info.pushConstantRangeCount = 1;
            layout_create_info.pPushConstantRanges = &constant_range;
            VK_CHECK_RESULT(vkCreatePipelineLayout(device->GetLogicalDevice(), &layout_create_info, nullptr, &m_SortPipelineLayout))
        }

        // Create Pipelines
        CompileAndCreatePipeline(m_ComputePipelines.FPS_Count, m_SortPipelineLayout);
        CompileAndCreatePipeline(m_ComputePipelines.FPS_CountReduce, m_SortPipelineLayout);
        CompileAndCreatePipeline(m_ComputePipelines.FPS_Scan, m_SortPipelineLayout);
        CompileAndCreatePipeline(m_ComputePipelines.FPS_ScanAdd, m_SortPipelineLayout);
        CompileAndCreatePipeline(m_ComputePipelines.FPS_Scatter, m_SortPipelineLayout);
        CompileAndCreatePipeline(m_ComputePipelines.FPS_ScatterPayload, m_SortPipelineLayout);
    //  CompileAndCreatePipeline(m_ComputePipelines.FPS_SetupIndirectParameters, "FPS_SetupIndirectParameters");

        // Create Key and Payload buffers
        m_DstKeyBuffers[0] = CreateRef<StorageBuffer>(sizeof(uint32_t) * m_MaxParticles, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        m_DstKeyBuffers[1] = CreateRef<StorageBuffer>(sizeof(uint32_t) * m_MaxParticles);
        m_DstPayloadBuffers[0] = CreateRef<StorageBuffer>(sizeof(uint32_t) * m_MaxParticles, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        m_DstPayloadBuffers[1] = CreateRef<StorageBuffer>(sizeof(uint32_t) * m_MaxParticles);

        // Allocate descriptor sets
 
        // Constants sets
        m_SortDescriptorSetConstants[0] = Renderer::AllocateDescriptorSet(m_ComputePipelines.FPS_Count.Shader->GetDescriptorSetLayout(0));
        m_SortDescriptorSetConstants[1] = Renderer::AllocateDescriptorSet(m_ComputePipelines.FPS_Count.Shader->GetDescriptorSetLayout(0));
        m_SortDescriptorSetConstants[2] = Renderer::AllocateDescriptorSet(m_ComputePipelines.FPS_Count.Shader->GetDescriptorSetLayout(0));

        // Input/Output sets
        m_SortDescriptorSetInputOutput[0] = Renderer::AllocateDescriptorSet(m_ComputePipelines.FPS_ScatterPayload.Shader->GetDescriptorSetLayout(2));
        m_SortDescriptorSetInputOutput[1] = Renderer::AllocateDescriptorSet(m_ComputePipelines.FPS_ScatterPayload.Shader->GetDescriptorSetLayout(2));

        // Scan sets
        m_SortDescriptorSetScanSets[0] = Renderer::AllocateDescriptorSet(m_ComputePipelines.FPS_ScanAdd.Shader->GetDescriptorSetLayout(3));
        m_SortDescriptorSetScanSets[1] = Renderer::AllocateDescriptorSet(m_ComputePipelines.FPS_ScanAdd.Shader->GetDescriptorSetLayout(3));

        // Scratch sets
        m_SortDescriptorSetScratch = Renderer::AllocateDescriptorSet(m_ComputePipelines.FPS_CountReduce.Shader->GetDescriptorSetLayout(4));

        // Indirect sets
    //  m_SortDescriptorSetIndirect = Renderer::AllocateDescriptorSet(m_ComputePipelines.FPS_ScanAdd.Shader->GetDescriptorSetLayout(5));

        VkBuffer BufferMaps[4];

        // Map inputs/outputs
        BufferMaps[0] = m_DstKeyBuffers[0]->GetBuffer();
        BufferMaps[1] = m_DstKeyBuffers[1]->GetBuffer();
        BufferMaps[2] = m_DstPayloadBuffers[0]->GetBuffer();
        BufferMaps[3] = m_DstPayloadBuffers[1]->GetBuffer();
        BindUAVBuffer(BufferMaps, m_SortDescriptorSetInputOutput[0], 0, 4);

        BufferMaps[0] = m_DstKeyBuffers[1]->GetBuffer();
        BufferMaps[1] = m_DstKeyBuffers[0]->GetBuffer();
        BufferMaps[2] = m_DstPayloadBuffers[1]->GetBuffer();
        BufferMaps[3] = m_DstPayloadBuffers[0]->GetBuffer();
        BindUAVBuffer(BufferMaps, m_SortDescriptorSetInputOutput[1], 0, 4);

        // Map scan sets (reduced, scratch)
        BufferMaps[0] = BufferMaps[1] = m_FPSReducedScratchBuffer->GetBuffer();
        BindUAVBuffer(BufferMaps, m_SortDescriptorSetScanSets[0], 0, 2);

        BufferMaps[0] = BufferMaps[1] = m_FPSScratchBuffer->GetBuffer();
        BufferMaps[2] = m_FPSReducedScratchBuffer->GetBuffer();
        BindUAVBuffer(BufferMaps, m_SortDescriptorSetScanSets[1], 0, 3);

        // Map Scratch areas (fixed)
        BufferMaps[0] = m_FPSScratchBuffer->GetBuffer();
        BufferMaps[1] = m_FPSReducedScratchBuffer->GetBuffer();
        BindUAVBuffer(BufferMaps, m_SortDescriptorSetScratch, 0, 2);

        // Map indirect buffers
    //	BufferMaps[0] = m_IndirectKeyCounts->GetBuffer();
    //	BufferMaps[1] = m_IndirectConstantBuffer->GetBuffer();
    //	BufferMaps[2] = m_IndirectCountScatterArgs->GetBuffer();
    //	BufferMaps[3] = m_IndirectReduceScanArgs->GetBuffer();
    //	BindUAVBuffer(BufferMaps, m_SortDescriptorSetIndirect, 0, 4);
    }

    Ref<StorageBuffer> ParticleSort::Sort(uint32_t count, Ref<StorageBuffer> distanceBuffer, Ref<StorageBuffer> indexBuffer)
    {
        Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();

        m_NumKeys = count;

        // Copy data into buffers (NOTE: Probably should just use the alreay existing buffer)
        // if (false)
        {
            VkCommandBuffer commandBuffer = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
            VkBufferCopy copyInfo = { 0 };
            copyInfo.srcOffset = 0;
            copyInfo.size = sizeof(uint32_t) * m_NumKeys;

            VkBufferMemoryBarrier barriers[2] = {
                BufferTransition(m_DstKeyBuffers[0]->GetBuffer(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, m_DstKeyBuffers[0]->GetSize()),
                BufferTransition(m_DstPayloadBuffers[0]->GetBuffer(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, m_DstPayloadBuffers[0]->GetSize())
            };

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 2, barriers, 0, nullptr);

            vkCmdCopyBuffer(commandBuffer, distanceBuffer->GetBuffer(), m_DstKeyBuffers[0]->GetBuffer(), 1, &copyInfo);
            vkCmdCopyBuffer(commandBuffer, indexBuffer->GetBuffer(), m_DstPayloadBuffers[0]->GetBuffer(), 1, &copyInfo);

            barriers[0] = BufferTransition(m_DstKeyBuffers[0]->GetBuffer(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, m_DstKeyBuffers[0]->GetSize());
            barriers[1] = BufferTransition(m_DstPayloadBuffers[0]->GetBuffer(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, m_DstPayloadBuffers[0]->GetSize());
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 2, barriers, 0, nullptr);

            device->FlushCommandBuffer(commandBuffer, true);
        }

        // m_DstKeyBuffers[0] = distanceBuffer;
        // m_DstPayloadBuffers[0] = indexBuffer;

        // Update descriptor sets (NOTE: Should not have to do this)
        {
            // Constants sets
            m_SortDescriptorSetConstants[0] = Renderer::AllocateDescriptorSet(m_ComputePipelines.FPS_Count.Shader->GetDescriptorSetLayout(0));
            m_SortDescriptorSetConstants[1] = Renderer::AllocateDescriptorSet(m_ComputePipelines.FPS_Count.Shader->GetDescriptorSetLayout(0));
            m_SortDescriptorSetConstants[2] = Renderer::AllocateDescriptorSet(m_ComputePipelines.FPS_Count.Shader->GetDescriptorSetLayout(0));

            // Input/Output sets
            m_SortDescriptorSetInputOutput[0] = Renderer::AllocateDescriptorSet(m_ComputePipelines.FPS_ScatterPayload.Shader->GetDescriptorSetLayout(2));
            m_SortDescriptorSetInputOutput[1] = Renderer::AllocateDescriptorSet(m_ComputePipelines.FPS_ScatterPayload.Shader->GetDescriptorSetLayout(2));

            // Scan sets
            m_SortDescriptorSetScanSets[0] = Renderer::AllocateDescriptorSet(m_ComputePipelines.FPS_ScanAdd.Shader->GetDescriptorSetLayout(3));
            m_SortDescriptorSetScanSets[1] = Renderer::AllocateDescriptorSet(m_ComputePipelines.FPS_ScanAdd.Shader->GetDescriptorSetLayout(3));

            // Scratch sets
            m_SortDescriptorSetScratch = Renderer::AllocateDescriptorSet(m_ComputePipelines.FPS_CountReduce.Shader->GetDescriptorSetLayout(4));

            VkBuffer BufferMaps[4];

            // Map inputs/outputs
            BufferMaps[0] = m_DstKeyBuffers[0]->GetBuffer();
            BufferMaps[1] = m_DstKeyBuffers[1]->GetBuffer();
            BufferMaps[2] = m_DstPayloadBuffers[0]->GetBuffer();
            BufferMaps[3] = m_DstPayloadBuffers[1]->GetBuffer();
            BindUAVBuffer(BufferMaps, m_SortDescriptorSetInputOutput[0], 0, 4);

            BufferMaps[0] = m_DstKeyBuffers[1]->GetBuffer();
            BufferMaps[1] = m_DstKeyBuffers[0]->GetBuffer();
            BufferMaps[2] = m_DstPayloadBuffers[1]->GetBuffer();
            BufferMaps[3] = m_DstPayloadBuffers[0]->GetBuffer();
            BindUAVBuffer(BufferMaps, m_SortDescriptorSetInputOutput[1], 0, 4);

            // Map scan sets (reduced, scratch)
            BufferMaps[0] = BufferMaps[1] = m_FPSReducedScratchBuffer->GetBuffer();
            BindUAVBuffer(BufferMaps, m_SortDescriptorSetScanSets[0], 0, 2);

            BufferMaps[0] = BufferMaps[1] = m_FPSScratchBuffer->GetBuffer();
            BufferMaps[2] = m_FPSReducedScratchBuffer->GetBuffer();
            BindUAVBuffer(BufferMaps, m_SortDescriptorSetScanSets[1], 0, 3);

            // Map Scratch areas (fixed)
            BufferMaps[0] = m_FPSScratchBuffer->GetBuffer();
            BufferMaps[1] = m_FPSReducedScratchBuffer->GetBuffer();
            BindUAVBuffer(BufferMaps, m_SortDescriptorSetScratch, 0, 2);
        }

        // Print pre-sorted buffer
        if (false) {
            CR_LOG_DEBUG("Pre-sorted:");
            {
                ScopedMap<uint32_t, StorageBuffer> buffer(m_DstKeyBuffers[0]);
                ScopedMap<uint32_t, StorageBuffer> buffer2(m_DstPayloadBuffers[0]);
                for (int i = 0; i < m_NumKeys; i++)
                {
                    CR_LOG_DEBUG("    [{0}] {1}", buffer2[i], buffer[i]);
                }
            }
        }

        VkCommandBuffer sortCommandBuffer = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        SortInternal(sortCommandBuffer);

        device->FlushCommandBuffer(sortCommandBuffer, true);

        // Print sorted buffer
		if (false) {
            CR_LOG_DEBUG("Sorted:");
            {
				ScopedMap<uint32_t, StorageBuffer> buffer(m_DstKeyBuffers[0]);
				ScopedMap<uint32_t, StorageBuffer> buffer2(m_DstPayloadBuffers[0]);
				for (int i = 0; i < m_NumKeys; i++)
				{
					CR_LOG_DEBUG("    [{0}] {1}", buffer2[i], buffer[i]);
				}
            }
        }

        return m_DstPayloadBuffers[0];
    }

    // Perform Parallel Sort (radix-based sort)
    void ParticleSort::SortInternal(VkCommandBuffer commandList)
    {
        constexpr uint32_t m_MaxNumThreadgroups = 800;

        bool bIndirectDispatch = false;

        // To control which descriptor set to use for updating data
        static uint32_t frameCount = 0;
        uint32_t frameConstants = (++frameCount) % 3;

        // Buffers to ping-pong between when writing out sorted values
        VkBuffer ReadBufferInfo = m_DstKeyBuffers[0]->GetBuffer();
        VkBuffer WriteBufferInfo = m_DstKeyBuffers[1]->GetBuffer();

        VkBuffer ReadPayloadBufferInfo = m_DstPayloadBuffers[0]->GetBuffer();
        VkBuffer WritePayloadBufferInfo = m_DstPayloadBuffers[1]->GetBuffer();

        bool bHasPayload = true;// m_UISortPayload;

        // Setup barriers for the run
        VkBufferMemoryBarrier Barriers[3];
        FFX_ParallelSortCB  constantBufferData = { 0 };

        // Fill in the constant buffer data structure (this will be done by a shader in the indirect version)
        uint32_t NumThreadgroupsToRun;
        uint32_t NumReducedThreadgroupsToRun;
        if (!bIndirectDispatch)
        {
            uint32_t NumberOfKeys = m_NumKeys;
            FFX_ParallelSort_SetConstantAndDispatchData(NumberOfKeys, m_MaxNumThreadgroups, constantBufferData, NumThreadgroupsToRun, NumReducedThreadgroupsToRun);
        }
        else
        {
#if 0
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
#endif
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
            FFX_ParallelSortCB* dst = m_ConstantBuffer->Map<FFX_ParallelSortCB>();
            memcpy(dst, &constantBufferData, sizeof(FFX_ParallelSortCB));
            m_ConstantBuffer->Unmap();

            VkDescriptorBufferInfo constantBuffer = m_ConstantBuffer->getDescriptorBufferInfo();
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
                    vkCmdDispatchIndirect(commandList, m_IndirectCountScatterArgs->GetBuffer(), 0);
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
            Barriers[numBarriers++] = BufferTransition(WriteBufferInfo, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, sizeof(uint32_t) * m_NumKeys);
            if (bHasPayload)
                Barriers[numBarriers++] = BufferTransition(WritePayloadBufferInfo, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, sizeof(uint32_t) * m_NumKeys);
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
		
        m_ConstantBuffer = CreateRef<UniformBuffer>(nullptr, sizeof(FFX_ParallelSortCB));

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
	}

    void ParticleSort::BindUAVBuffer(VkBuffer* pBuffer, VkDescriptorSet& DescriptorSet, uint32_t Binding/*=0*/, uint32_t Count/*=1*/)
    {
        VkDevice device = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();

        std::vector<VkDescriptorBufferInfo> bufferInfos;
        for (uint32_t i = 0; i < Count; i++)
        {
            VkDescriptorBufferInfo bufferInfo;
            bufferInfo.buffer = pBuffer[i];
            bufferInfo.offset = 0;
            bufferInfo.range = VK_WHOLE_SIZE;
            bufferInfos.push_back(bufferInfo);
        }

        VkWriteDescriptorSet write_set = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        write_set.pNext = nullptr;
        write_set.dstSet = DescriptorSet;
        write_set.dstBinding = Binding;
        write_set.dstArrayElement = 0;
        write_set.descriptorCount = Count;
        write_set.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write_set.pImageInfo = nullptr;
        write_set.pBufferInfo = bufferInfos.data();
        write_set.pTexelBufferView = nullptr;

        vkUpdateDescriptorSets(device, 1, &write_set, 0, nullptr);
    }

    void ParticleSort::CompileShader(SortPipeline& pipeline, std::string_view entryPoint, const std::string& define /*= ""*/)
    {
        if (!define.empty())
        {
            std::vector<std::wstring> defines = { std::wstring(define.begin(), define.end()) };
            pipeline.Shader = CreateRef<Shader>("assets/shaders/sorting/ParallelSortCS.hlsl", entryPoint, defines);
        }
        else
        {
            pipeline.Shader = CreateRef<Shader>("assets/shaders/sorting/ParallelSortCS.hlsl", entryPoint);
        }
    }
    
    void ParticleSort::CompileAndCreatePipeline(SortPipeline& pipeline, VkPipelineLayout layout)
    {
        pipeline.Pipeline = CreateRef<VulkanComputePipeline>(pipeline.Shader, layout);
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

}