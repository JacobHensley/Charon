#pragma once
#include "Charon/Core/Layer.h"
#include "Charon/Core/Core.h"
#include "Charon/Graphics/Camera.h"
#include "Charon/Graphics/Mesh.h"
#include "Charon/Graphics/Buffers.h"
#include "Charon/Graphics/Shader.h"
#include "Charon/Graphics/VulkanPipeline.h"

namespace Charon {

    class ParticleLayer : public Layer
    {
    public:
        ParticleLayer();
        ~ParticleLayer();

    public:
        void Init();

        void OnUpdate();
        void OnRender();
        void OnImGUIRender();

    private:
        Ref<Camera> m_Camera;

        uint32_t m_MaxQauds;
        uint32_t m_MaxIndices;

        Ref<StorageBuffer> m_StorageBuffer;
        Ref<IndexBuffer> m_IndexBuffer;
        Ref<Shader> m_ComputeShader;
        Ref<VulkanPipeline> m_Pipeline;
        Ref<Shader> m_Shader;

        Vertex* m_Vertices = nullptr;
        uint16_t* m_Indices = nullptr;

        VkDescriptorSetLayout m_ComputeDescriptorSetLayout = nullptr;
        VkDescriptorSet m_ComputeDescriptorSet = nullptr;

        VkPipelineLayout m_ComputePipelineLayout = nullptr;
        VkPipeline m_ComputePipeline = nullptr;
    };

}