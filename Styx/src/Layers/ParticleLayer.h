#pragma once
#include "Charon/Core/Layer.h"
#include "Charon/Core/Core.h"
#include "Charon/Graphics/Camera.h"
#include "Charon/Graphics/VulkanComputePipeline.h"
#include "Charon/Graphics/VulkanPipeline.h"
#include "Charon/Graphics/Buffers.h"

namespace Charon {

    struct Particle
    {
        glm::vec3 Position;
        float Padding0;
        glm::vec3 Color;
        float Padding1;
        glm::vec3 Velocity;
        float RemainingLifetime;
    };

    struct ParticleVertex
    {
        glm::vec3 Position;
        float Padding0;
    };

    struct CounterBuffer
    {
        float m_ParticleCount;
    };

    struct Emitter
    {
        glm::vec3 EmitterPosition;
        float Padding0;
        glm::vec3 EmitterDirection;
        uint32_t EmissionQuantity;
        uint32_t MaxParticles;

        // Move to renderer buffer
        float Time;
        float DeltaTime;
    };

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

        Ref<VulkanComputePipeline> m_ComputePipeline;
        Ref<Shader> m_ComputeShader;

        Ref<VulkanPipeline> m_Pipeline;
        Ref<Shader> m_ParticleShader;

        Ref<StorageBuffer> m_ParticleBuffer;
        Ref<StorageBuffer> m_ParticleVertexBuffer;
        Ref<StorageBuffer> m_CounterBuffer;
        Ref<UniformBuffer> m_EmitterUniformBuffer;
        Ref<IndexBuffer> m_IndexBuffer;

        Emitter m_Emitter;

        uint32_t m_MaxQauds = 0;
        uint32_t m_MaxIndices = 0;

        uint32_t m_ParticleCount = 0;

        VkDescriptorSet m_ComputeDescriptorSet = nullptr;
        std::vector<VkWriteDescriptorSet> m_ComputeWriteDescriptors;

        VkDescriptorSet m_ParticleDescriptorSet = nullptr;
        std::vector<VkWriteDescriptorSet> m_ParticleWriteDescriptors;
        
    };

}