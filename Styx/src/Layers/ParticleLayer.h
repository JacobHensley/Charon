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
		uint32_t AliveCount = 0;
		uint32_t DeadCount = 0;
		uint32_t RealEmitCount = 0;
		uint32_t AliveCount_AfterSimulation = 0;
    };


	struct IndirectDrawBuffer
	{
        glm::uvec3 DispatchEmit{ 0,0,0 };
        uint32_t Padding0;
		glm::uvec3 DispatchSimulation{ 0,0,0 };
        uint32_t Padding1;
        glm::uvec4 DrawParticles{ 0,0,0,0 };
        uint32_t FirstInstance = 0;
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

        Ref<VulkanPipeline> m_Pipeline;
        Ref<Shader> m_ParticleShader;

        Ref<StorageBuffer> m_ParticleBuffer;
        Ref<StorageBuffer> m_ParticleVertexBuffer;
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

        uint32_t m_MaxIndices = 0;

        uint32_t m_ParticleCount = 0;

        inline static uint32_t m_MaxParticles = 1'000'000;

        VkDescriptorSet m_ParticleDescriptorSet = nullptr;
        std::vector<VkWriteDescriptorSet> m_ParticleWriteDescriptors;

		struct DebugBuffers
		{
            NewCounterBuffer m_NewCounterBuffer{};
			IndirectDrawBuffer m_IndirectDrawBuffer{};
            uint32_t* m_DeadBuffer = new uint32_t[m_MaxParticles];
		} m_DebugBuffers[4];

        void DrawParticleBuffers(int index);

        bool m_NextFrame = false;
    };

}