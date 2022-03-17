#pragma once
#include "Charon/Core/Layer.h"
#include "Charon/Core/Core.h"
#include "Charon/Graphics/Camera.h"
#include "Charon/Graphics/VulkanComputePipeline.h"
#include "Charon/Graphics/VulkanPipeline.h"
#include "Charon/Graphics/Buffers.h"
#include "Charon/Graphics/Texture2D.h"
#include "UI/ViewportPanel.h"
#include "UI/ImGuiColorGradient.h"

#include "Particles/ParticleSort.h"

namespace Charon {

    struct Particle
    {
        glm::vec3 Position;
        float CurrentLife;
        glm::vec3 Rotation;
        float Speed;
        glm::vec3 Scale;
        float Lifetime;
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
        uint32_t AliveCount = 0;
        uint32_t DeadCount = 0;
        uint32_t RealEmitCount = 0;
        uint32_t AliveCount_AfterSimulation = 0;
    };

	struct GradientPoint
	{
		glm::vec3 Color;
		float Position;
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

		uint32_t GradientPointCount;
		uint32_t Padding0;
		GradientPoint ColorGradientPoints[10];

        float Time;
        float DeltaTime;
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

    struct SortParameters
    {
		uint32_t h;
		uint32_t algorithm;
		uint32_t workgroupSizeX;
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
        Ref<ViewportPanel> m_ViewportPanel;
        Emitter m_Emitter;
        SortParameters m_SortParameters;

        inline static uint32_t m_MaxParticles = 20;
        inline static uint32_t m_MaxIndices = m_MaxParticles * 6;

        float m_EmitCount = 0.0f;
        float m_Count = 0.0f;

        float m_Burst = 0.0f;
        float m_BurstCount = 0.0f;
        float m_BurstInterval = 0.0f;
        float m_BurstIntervalCount = 0.0f;

        float m_SecondTimer = 1.0f;
        uint32_t m_ParticlesEmittedPerSecond = 0;
        
        // Stats
        uint32_t m_ParticleCount = 0;

        bool m_Sort = false;

        ImGradient m_ColorLifetimeGradient;

        Ref<VulkanPipeline> m_ParticleRendererPipeline;
        Ref<Shader> m_ParticleShader;

        VkDescriptorSet m_ParticleRendererDescriptorSet = nullptr;
        std::vector<VkWriteDescriptorSet> m_ParticleRendererWriteDescriptors;

        VkDescriptorSet m_ParticleSimulationDescriptorSet = nullptr;
        std::vector<VkWriteDescriptorSet> m_ParticleSimulationWriteDescriptors;

		VkDescriptorSet m_ParticleSortDescriptorSet = nullptr;
		std::vector<VkWriteDescriptorSet> m_ParticleSortWriteDescriptors;

        ParticleSort m_ParticleSort;

        struct ParticleShaders
        {
            Ref<Shader> Begin;
            Ref<Shader> Emit;
            Ref<Shader> Simulate;
            Ref<Shader> End;
            Ref<Shader> Sort;
        } m_ParticleShaders;

        struct ParticlePipelines
        {
            Ref<VulkanComputePipeline> Begin;
            Ref<VulkanComputePipeline> Emit;
            Ref<VulkanComputePipeline> Simulate;
            Ref<VulkanComputePipeline> End;
            Ref<VulkanComputePipeline> Sort;

        } m_ParticlePipelines;

        struct ParticleBuffers
        {
            Ref<StorageBuffer> ParticleBuffer;
            Ref<UniformBuffer> EmitterBuffer;
            Ref<StorageBuffer> AliveBufferPreSimulate;
            Ref<StorageBuffer> AliveBufferPostSimulate;
            Ref<StorageBuffer> DeadBuffer;
            Ref<StorageBuffer> CounterBuffer;
            Ref<StorageBuffer> IndirectDrawBuffer;
            Ref<StorageBuffer> VertexBuffer;
            Ref<IndexBuffer> IndexBuffer;
			Ref<UniformBuffer> SortParameters;
        } m_ParticleBuffers;

        // Debug stuff
        bool m_Pause = false;
        bool m_NextFrame = false;
        bool m_Emit10 = false;
    };

}