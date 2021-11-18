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
	float Lifetime;
	vec3 Rotation;
	float Speed;
	vec3 Scale;
	vec3 Color;
	vec3 Velocity;
};

layout(std140, binding = 0) buffer ParticleBuffer
{
	Particle particles[];
} u_ParticleBuffer;

layout(std140, binding = 1) buffer AliveBufferPreSimulate
{
	uint Indices[];
} u_AliveBufferPreSimulate;

layout(std140, binding = 2) buffer AliveBufferPostSimulate
{
	uint Indices[];
} u_AliveBufferPostSimulate;

layout(std140, binding = 3) buffer DeadBuffer
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
} u_IndirectDrawBuffer;

layout(std140, binding = 7) uniform ParticleEmitter
{
	vec3 InitialRotation;
	float InitialLifetime;
	vec3 InitialScale;
	float InitialSpeed;
	vec3 InitialColor;
	float Gravity;
	vec3 Position;
	float EmissionRate;
	vec3 Direction;
	uint MaxParticles;
	float DirectionrRandomness;
	float VelocityRandomness;

	float Time;
	float DeltaTime;
} u_Emitter;

layout(binding = 8) uniform CameraBuffer
{
	mat4 ViewProjection;
	mat4 InverseViewProjection;
	mat4 View;
} u_CameraBuffer;

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

layout(local_size_x = THREADCOUNT_EMIT) in;
void main()
{
	uint particleIndex = gl_GlobalInvocationID.x;

	if (particleIndex < u_CounterBuffer.RealEmitCount)
	{
		vec2 uv = vec2(u_Emitter.DeltaTime, float(particleIndex) / float(THREADCOUNT_EMIT));
		float seed = 0.12345;

		// create new particle:
		Particle particle;
		particle.Position = u_Emitter.Position;
		particle.Lifetime = u_Emitter.InitialLifetime;
		particle.Rotation = u_Emitter.InitialRotation;
		particle.Speed = u_Emitter.InitialSpeed;
		particle.Scale = u_Emitter.InitialScale;
		particle.Color = u_Emitter.InitialColor;
		particle.Velocity = (u_Emitter.Direction * u_Emitter.InitialSpeed) + vec3(rand(seed, uv) - 0.5f, rand(seed, uv) - 0.5f, rand(seed, uv) - 0.5f) * u_Emitter.DirectionrRandomness;

		// new particle index retrieved from dead list (pop):
		uint deadCount = atomicCounterDecrement(u_CounterBuffer.DeadCount);
		uint newParticleIndex = u_DeadBuffer[deadCount - 1];

		// write out the new particle:
		u_ParticleBuffer[newParticleIndex] = particle;

		// and add index to the alive list (push):
		uint aliveCount = atomicCounterIncrement(u_CounterBuffer.AliveCount);
		u_AliveBufferPreSimulate[aliveCount] = newParticleIndex;
	}
}