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
        float Lifetime;
        glm::vec3 Rotation;
        float Speed;
        glm::vec3 Scale;
        float padding0;
        glm::vec3 Color;
        float padding1;
        glm::vec3 Velocity;
        float padding2;
    };

    struct ParticleVertex
    {
        glm::vec3 Position;
        float Padding0;
    };

    struct CounterBuffer
    {
        float AliveParticleCount;
        float TimeSinceLastEmit;
    };

    struct Emitter
    {
        glm::vec3 InitialRotation;
        float InitialLifetime;
        glm::vec3 InitialScale;
        float InitialSpeed;
        glm::vec3 InitialColor;
        float Gravity;
        glm::vec3 Position;
        uint32_t EmissionQuantity;
        glm::vec3 Direction;
        uint32_t MaxParticles;
        float DirectionrRandomness;
        float VelocityRandomness;
        float Time;
        float DeltaTime;
    };

    struct NewCounterBuffer
    {
		uint32_t AliveCount;
		uint32_t DeadCount;
		uint32_t RealEmitCount;
		uint32_t AliveCount_AfterSimulation;
    };

	struct IndirectDrawBuffer
	{
		glm::uvec3 DispatchEmit;
		glm::uvec3 DispatchSimulation;
        glm::uvec4 DrawParticles;
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
        
        Ref<Shader> m_ShaderParticleBegin;
        Ref<Shader> m_ShaderParticleEmit;
        Ref<Shader> m_ShaderParticleSimulate;
        Ref<Shader> m_ShaderParticleEnd;

        struct ParticlePipelines
        {
            Ref<VulkanComputePipeline> Begin;
            Ref<VulkanComputePipeline> Emit;
            Ref<VulkanComputePipeline> Simulate;
            Ref<VulkanComputePipeline> End;

        } m_ParticlePipelines;

        struct ParticleBuffers
        {
            Ref<StorageBuffer> AliveBufferPreSimulate;
            Ref<StorageBuffer> AliveBufferPostSimulate;
            Ref<StorageBuffer> DeadBuffer;
            Ref<StorageBuffer> CounterBuffer;
            Ref<StorageBuffer> IndirectDrawBuffer;
            Ref<UniformBuffer> ParticleUniformEmitter;
        } m_ParticleBuffers;
        
        VkDescriptorSet m_NewParticleDescriptorSet = nullptr;
        std::vector<VkWriteDescriptorSet> m_NewParticleWriteDescriptors;

        Emitter m_Emitter;

        uint32_t m_MaxQauds = 0;
        uint32_t m_MaxIndices = 0;

        uint32_t m_ParticleCount = 0;

        uint32_t m_MaxParticles = 1000000;

        VkDescriptorSet m_ComputeDescriptorSet = nullptr;
        std::vector<VkWriteDescriptorSet> m_ComputeWriteDescriptors;

        VkDescriptorSet m_ParticleDescriptorSet = nullptr;
        std::vector<VkWriteDescriptorSet> m_ParticleWriteDescriptors;
        
    };

}