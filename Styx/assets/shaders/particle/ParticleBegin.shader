#Shader Compute
#version 450

/////////////////////////////
// Move to include  v
/////////////////////////////

struct Vertex
{
	vec3 Position;
};

struct Particle
{
	vec3 Position;
	float CurrentLife;
	vec3 Rotation;
	float Speed;
	vec3 Scale;
	float Lifetime;
	vec3 Color;
	vec3 Velocity;
};

layout(std430, binding = 0) buffer ParticleBuffer
{
	Particle particles[];
} u_ParticleBuffer;

layout(std430, binding = 1) buffer AliveBufferPreSimulate
{
	uint Indices[];
} u_AliveBufferPreSimulate;

layout(std430, binding = 2) buffer AliveBufferPostSimulate
{
	uint Indices[];
} u_AliveBufferPostSimulate;

layout(std430, binding = 3) buffer DeadBuffer
{
	uint Indices[];
} u_DeadBuffer;

layout(std140, binding = 4) buffer VertexBuffer
{
	Vertex vertices[];
} u_VertexBuffer;

layout(std140, binding = 5) buffer CounterBuffer
{
	uint AliveCount;
	uint DeadCount;
	uint RealEmitCount;
	uint AliveCount_AfterSimulation;
} u_CounterBuffer;

layout(std140, binding = 6) buffer IndirectDrawBuffer
{
	uvec3 DispatchEmit;
	uvec3 DispatchSimulation;
	uvec4 DrawParticles;
	uint FirstInstance;
} u_IndirectDrawBuffer;

struct GradientPoint
{
	vec3 Color;
	float Position;
};

layout(std140, binding = 7) uniform ParticleEmitter
{
	vec3 InitialRotation;
	float InitialLifetime;
	vec3 InitialScale;
	float InitialSpeed;
	vec3 InitialColor;
	float Gravity;
	vec3 Position;
	uint EmissionQuantity;
	vec3 Direction;
	uint MaxParticles;
	float DirectionRandomness;
	float VelocityRandomness;

	uint GradientPointCount;
	uint Padding0;
	GradientPoint ColorGradientPoints[10];

	float Time;
	float DeltaTime;
} u_Emitter;

layout(binding = 8) uniform CameraBuffer
{
	mat4 ViewProjection;
	mat4 InverseViewProjection;
	mat4 View;
	mat4 InverseView;
} u_CameraBuffer;

layout(std430, binding = 9) buffer CameraDistanceBuffer
{
	uint m_DistanceToCamera[];
} u_CameraDistanceBuffer;

float rand(inout float seed, in vec2 uv)
{
	float result = fract(sin(seed * dot(uv, vec2(12.9898, 78.233))) * 43758.5453);
	seed += 1;
	return result;
}

const uint THREADCOUNT_EMIT = 256;
const uint THREADCOUNT_SIMULATION = 256;

/////////////////////////////
// Move to include  ^
/////////////////////////////

layout(local_size_x = 1) in;
void main()
{
	// we can not emit more than there are free slots in the dead list:
	uint realEmitCount = min(u_CounterBuffer.DeadCount, u_Emitter.EmissionQuantity);

	// Fill dispatch argument buffer for emitting (ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ):
	u_IndirectDrawBuffer.DispatchEmit = uvec3(ceil(float(realEmitCount) / float(THREADCOUNT_EMIT)), 1, 1);

	// Fill dispatch argument buffer for simulation (ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ):
	u_IndirectDrawBuffer.DispatchSimulation = uvec3(ceil(float(u_CounterBuffer.AliveCount_AfterSimulation + realEmitCount) / float(THREADCOUNT_SIMULATION)), 1, 1);

	// copy new alivelistcount to current alivelistcount: (from previous frame)
	u_CounterBuffer.AliveCount = u_CounterBuffer.AliveCount_AfterSimulation;

	// reset new alivecount:
	u_CounterBuffer.AliveCount_AfterSimulation = 0;

	// and write real emit count:
	u_CounterBuffer.RealEmitCount = realEmitCount;

}