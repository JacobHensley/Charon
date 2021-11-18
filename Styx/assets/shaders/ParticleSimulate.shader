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

layout(local_size_x = THREADCOUNT_SIMULATION) in;
void main()
{
	uint invocationID = gl_GlobalInvocationID.x;

	if (invocationID < u_CounterBuffer.AliveCount)
	{
		uint particleIndex = u_AliveBufferPreSimulate[invocationID];
		Particle particle = u_ParticleBuffer[particleIndex];
		uint v0 = particleIndex * 4;

		if (particle.Lifetime > 0)
		{
			// TODO: -----> Simulate Here <-----

			// write back simulated particle:
			u_ParticleBuffer[particleIndex] = particle;

			// add to new alive list:
			uint newAliveIndex = atomicCounterIncrement(u_CounterBuffer.AliveCount_AfterSimulation);
			u_AliveBufferPostSimulate[newAliveIndex] = particleIndex;

			// Write out render buffers:
			// These must be persistent, not culled (raytracing, surfels...)

			for (uint vertexID = 0; vertexID < 4; ++vertexID)
			{
				// write out vertex:
				vertices[(v0 + vertexID)].Position = particle.position + vec3(-0.5, -0.5, 0.0);
				vertices[(v0 + vertexID)].Position = particle.position + vec3(0.5, -0.5, 0.0);
				vertices[(v0 + vertexID)].Position = particle.position + vec3(0.5, 0.5, 0.0);
				vertices[(v0 + vertexID)].Position = particle.position + vec3(-0.5, 0.5, 0.0);
			}

		}
		else
		{
			// kill:
			uint deadIndex = atomicCounterIncrement(u_CounterBuffer.DeadCount);
			u_DeadBuffer[deadIndex] = particleIndex;

			u_VertexBuffer[(v0 + 0)] = 0;
			u_VertexBuffer[(v0 + 1)] = 0;
			u_VertexBuffer[(v0 + 2)] = 0;
			u_VertexBuffer[(v0 + 3)] = 0;
		}
	}

}