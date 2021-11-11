#Shader Compute
#version 450

struct Vertex
{
	vec3 Position;
};

struct Particle
{
	vec3 Position;
	vec3 Color;
	vec3 Velocity;
	float RemainingLifetime;
};

layout(std140, binding = 0) buffer ParticleBuffer
{
	Particle particles[];
};

layout(std140, binding = 1) buffer VertexBuffer
{
	Vertex vertices[];
};

layout(std140, binding = 2) uniform ParticleEmitter
{
	vec3 EmitterPosition;
	vec3 EmitterDirection;
	uint EmissionQuantity;
	uint MaxParticles;

	// Move to renderer UB
	float Time;
	float DeltaTime;
} u_Emitter;

layout(std140, binding = 3) buffer CounterBuffer
{
	uint ParticleCount;
};

layout(binding = 4) uniform CameraBuffer
{
	mat4 ViewProjection;
	mat4 InverseViewProjection;
	mat4 View;
} u_CameraBuffer;

vec3 hash31(float p)
{
	vec3 p3 = fract(vec3(p) * vec3(.1031, .1030, .0973));
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.xxy + p3.yzz) * p3.zyx);
}

vec3 RandomVec3()
{
	return hash31(u_Emitter.Time + gl_GlobalInvocationID.x);
}

vec3 RandomVec3(float offset)
{
	return hash31(u_Emitter.Time + gl_GlobalInvocationID.x + offset);
}

void SetQuadPosition(uint index, vec3 position)
{
	vertices[index * 4 + 0].Position = position + vec3(-0.5, -0.5, 0.0);
	vertices[index * 4 + 1].Position = position + vec3( 0.5, -0.5, 0.0);
	vertices[index * 4 + 2].Position = position + vec3( 0.5,  0.5, 0.0);
	vertices[index * 4 + 3].Position = position + vec3(-0.5,  0.5, 0.0);
}

bool IsParticleDead(uint index)
{
	return particles[index].RemainingLifetime <= 0.0f;
}

void EmitParticle(uint index, Particle particle)
{
	particles[index] = particle;
	SetQuadPosition(index, particle.Position);
	ParticleCount = ParticleCount + 1;
}

void UpdateParticle(uint index)
{
	particles[index].Position += particles[index].Velocity;
	particles[index].RemainingLifetime -= u_Emitter.DeltaTime;
	particles[index].Color = vec3(particles[index].RemainingLifetime) / 5.0f;
	SetQuadPosition(index, particles[index].Position);
}

layout(local_size_x = 1) in;
void main()
{
	uint particleIndex = gl_GlobalInvocationID.x;

	for (uint i = 0; i < u_Emitter.EmissionQuantity; i++)
	{
		if (ParticleCount <= u_Emitter.MaxParticles - 1)
		{	
			uint index = i + ParticleCount - u_Emitter.EmissionQuantity;

			Particle particle;
			particle.Color = vec3(1.0f);
			particle.Position = vec3(0.0f);
			particle.Velocity = RandomVec3(index) * 0.001f;
			particle.RemainingLifetime = 5.0f;

			EmitParticle(index, particle);
		}
	}

	for (uint i = 0; i < ParticleCount; i++)
	{
		UpdateParticle(i);
	}
		
	for (uint i = 0; i < ParticleCount; i++)
	{
		if (IsParticleDead(i))
		{
			Particle deadParticle = particles[i];
			particles[i] = particles[ParticleCount - 1];
			particles[ParticleCount - 1] = deadParticle;

			ParticleCount = ParticleCount - 1;
		}
	}
}