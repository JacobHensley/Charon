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

    struct ParticleDrawDetails
    {
        uint32_t IndexOffset = 0;
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
        float DirectionRandomness;
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
		int GetGraphIndex(const std::vector<glm::vec2>& bezierCubicPoints, float x);
    private:
        // Display
        Ref<Camera> m_Camera;
        Ref<ViewportPanel> m_ViewportPanel;

        // Default emitter
        Emitter m_Emitter;
        ParticleDrawDetails m_ParticleDrawDetails;

        // Total max particles in the world
        inline static uint32_t m_MaxParticles = 1'000'000;
        inline static uint32_t m_MaxIndices = m_MaxParticles * 6;

        float m_RequestedParticlesPerFrame = 0.0f;
        float m_RequestedParticlesPerSecond = 0.0f;
        
        ImGradient m_ColorLifetimeGradient;

        Ref<ParticleSort> m_ParticleSort;

        // Particle rendering
        Ref<VulkanPipeline> m_ParticleRendererPipeline;
        Ref<Shader> m_ParticleShader;

        // DescriptorSet for rendering
        VkDescriptorSet m_ParticleRendererDescriptorSet = nullptr;
        std::vector<VkWriteDescriptorSet> m_ParticleRendererWriteDescriptors;

        // DescriptorSet for simulation
        VkDescriptorSet m_ParticleSimulationDescriptorSet = nullptr;
        std::vector<VkWriteDescriptorSet> m_ParticleSimulationWriteDescriptors;

        struct ParticleShaders
        {
            Ref<Shader> Begin;
            Ref<Shader> Emit;
            Ref<Shader> Simulate;
            Ref<Shader> End;
        } m_ParticleShaders;

        struct ParticlePipelines
        {
            Ref<VulkanComputePipeline> Begin;
            Ref<VulkanComputePipeline> Emit;
            Ref<VulkanComputePipeline> Simulate;
            Ref<VulkanComputePipeline> End;
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
            Ref<StorageBuffer> CameraDistanceBuffer;
            Ref<StorageBuffer> DrawParticleIndexBuffer;
			Ref<UniformBuffer> ParticleDrawDetails;
            Ref<IndexBuffer> IndexBuffer;
        } m_ParticleBuffers;

        Ref<Mesh> m_DebugSphere;
		std::vector<glm::vec2> m_BezierCubicPoints;

        // Debug settings
        bool m_EnableSorting = true;
        bool m_Pause = false;
        bool m_NextFrame = false;
    };

}