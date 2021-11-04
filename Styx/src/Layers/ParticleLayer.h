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

        struct ParticleVertex
        {
            glm::vec3 Position;
            float Padding0;
            glm::vec3 Color;
            float Padding1;
			glm::vec3 Velocity;
			float Padding2;
        };

        Ref<StorageBuffer> m_StorageBuffer;

        struct ParticleUB
        {
			glm::vec3 EmitterPosition;
            float Padding0;
			glm::vec3 EmitterDirection;
			float Padding1;
			glm::vec3 EmitterDirectionVariation;
			uint32_t EmissionQuantity; // how many new particles to emit
			uint32_t MaxParticles;
			uint32_t ParticleCount = 0;
			float Time;
        } m_ParticleUB;
        Ref<UniformBuffer> m_ParticleUniformBuffer;
        Ref<IndexBuffer> m_IndexBuffer;
        Ref<Shader> m_ComputeShader;
        Ref<VulkanPipeline> m_Pipeline;
        Ref<Shader> m_Shader;

        std::vector<VkWriteDescriptorSet> m_ComputeWriteDescriptors;

        Vertex* m_Vertices = nullptr;
        uint16_t* m_Indices = nullptr;

        VkDescriptorSetLayout m_ComputeDescriptorSetLayout = nullptr;
        VkDescriptorSet m_ComputeDescriptorSet = nullptr;

        VkPipelineLayout m_ComputePipelineLayout = nullptr;
        VkPipeline m_ComputePipeline = nullptr;
    };

}