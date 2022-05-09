#Shader Compute
#version 450

/////////////////////////////
// Move to include  v
/////////////////////////////

struct Vertex
{
	vec3 Position;
	float Padding;
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
	float DirectionrRandomness;
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
} u_CameraBuffer;

layout(std430, binding = 9) buffer CameraDistanceBuffer
{
	uint DistanceToCamera[];
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

vec3 GetParticleColor(float lifePercentage, vec3 particleColor)
{
	if (u_Emitter.GradientPointCount == 0)
		return particleColor;

	// Gradient position == lifePercentage
	float position = clamp(lifePercentage, 0.0f, 1.0f);

	GradientPoint lower;
	lower.Position = -1.0f;
	GradientPoint upper;
	upper.Position = -1.0f;

	// If only 1 point, or we're beneath the first point, return the first point
	if (u_Emitter.GradientPointCount == 1 || position <= u_Emitter.ColorGradientPoints[0].Position)
		return u_Emitter.ColorGradientPoints[0].Color;

	// If we're beyond the last point, return the last point
	if (position >= u_Emitter.ColorGradientPoints[u_Emitter.GradientPointCount - 1].Position)
		return u_Emitter.ColorGradientPoints[u_Emitter.GradientPointCount - 1].Color;

	for (int i = 0; i < u_Emitter.GradientPointCount; i++)
	{
		if (u_Emitter.ColorGradientPoints[i].Position <= position)
		{
			if (lower.Position == -1.0f || lower.Position <= u_Emitter.ColorGradientPoints[i].Position)
				lower = u_Emitter.ColorGradientPoints[i];
		}

		if (u_Emitter.ColorGradientPoints[i].Position >= position)
		{
			if (upper.Position == -1.0f || upper.Position >= u_Emitter.ColorGradientPoints[i].Position)
				upper = u_Emitter.ColorGradientPoints[i];
		}
	}

	float distance = upper.Position - lower.Position;
	float delta = (position - lower.Position) / distance;

	// lerp
	float r = ((1.0f - delta) * lower.Color.r) + ((delta) * upper.Color.r);
	float g = ((1.0f - delta) * lower.Color.g) + ((delta) * upper.Color.g);
	float b = ((1.0f - delta) * lower.Color.b) + ((delta) * upper.Color.b);

	return vec3(r, g, b);
}

layout(local_size_x = THREADCOUNT_SIMULATION) in;
void main()
{
	uint invocationID = gl_GlobalInvocationID.x;

	if (invocationID < u_CounterBuffer.AliveCount)
	{
		uint particleIndex = u_AliveBufferPreSimulate.Indices[invocationID];
		Particle particle = u_ParticleBuffer.particles[particleIndex];
		uint v0 = particleIndex * 4;

		if (particle.CurrentLife > 0)
		{
			// simulate
			particle.Position += particle.Velocity * u_Emitter.DeltaTime;
			particle.Velocity.y -= u_Emitter.Gravity * u_Emitter.DeltaTime;
			particle.CurrentLife -= u_Emitter.DeltaTime;
			particle.Color = GetParticleColor(1.0f - particle.CurrentLife / particle.Lifetime, particle.Color);
			
			// write back simulated particle:
			u_ParticleBuffer.particles[particleIndex] = particle;

			// add to new alive list:
			uint newAliveIndex = atomicAdd(u_CounterBuffer.AliveCount_AfterSimulation, 1);
			u_AliveBufferPostSimulate.Indices[newAliveIndex] = particleIndex;

			// write out vertex:
			u_VertexBuffer.vertices[(v0 + 0)].Position = particle.Position + vec3(-0.5, -0.5, 0.0);
			u_VertexBuffer.vertices[(v0 + 1)].Position = particle.Position + vec3(0.5, -0.5, 0.0);
			u_VertexBuffer.vertices[(v0 + 2)].Position = particle.Position + vec3(0.5, 0.5, 0.0);
			u_VertexBuffer.vertices[(v0 + 3)].Position = particle.Position + vec3(-0.5, 0.5, 0.0);

			// store squared distance to main camera:
			vec3 cameraPosition = inverse(u_CameraBuffer.View)[3].xyz; // TODO: Invert view matrix in C++
			float particleDist = length(particle.Position - cameraPosition);
			uint distSQ = -uint(particleDist * 1000000.0f);
			u_CameraDistanceBuffer.DistanceToCamera[newAliveIndex] = distSQ;
		}
		else
		{
			// kill:
			uint deadIndex = atomicAdd(u_CounterBuffer.DeadCount, 1);
			u_DeadBuffer.Indices[deadIndex] = particleIndex;

			u_VertexBuffer.vertices[(v0 + 0)].Position = vec3(0);
			u_VertexBuffer.vertices[(v0 + 1)].Position = vec3(0);
			u_VertexBuffer.vertices[(v0 + 2)].Position = vec3(0);
			u_VertexBuffer.vertices[(v0 + 3)].Position = vec3(0);
		}
	}

}