#Shader Compute
#version 450

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
};

layout(std140, binding = 1) buffer VertexBuffer
{
	Vertex vertices[];
};

layout(std140, binding = 2) uniform ParticleEmitter
{
	vec3 InitialRotation;
	float InitialLifetime;
	vec3 InitialScale;
	float InitialSpeed;
	vec3 InitialColor;
	float Gravity;
	vec3 Position;
	float EmissionRate; // Particles to emit per second
	vec3 Direction;
	uint MaxParticles;
	float DirectionrRandomness;
	float VelocityRandomness;

	// Move to renderer UB
	float Time;
	float DeltaTime;
} u_Emitter;

layout(std140, binding = 3) buffer CounterBuffer
{
	uint ParticleCount;
	float TimeSinceLastEmit;
};

layout(binding = 4) uniform CameraBuffer
{
	mat4 ViewProjection;
	mat4 InverseViewProjection;
	mat4 View;
} u_CameraBuffer;

// returns a random float in range (0, 1). seed must be >0!
float rand(inout float seed, in vec2 uv)
{
	float result = fract(sin(seed * dot(uv, vec2(12.9898, 78.233))) * 43758.5453);
	seed += 1;
	return result;
}

void SetQuadPosition(uint index, vec3 position, float scale)
{
	vertices[index * 4 + 0].Position = position + vec3(-0.5, -0.5, 0.0) * scale;
	vertices[index * 4 + 1].Position = position + vec3( 0.5, -0.5, 0.0) * scale;
	vertices[index * 4 + 2].Position = position + vec3( 0.5,  0.5, 0.0) * scale;
	vertices[index * 4 + 3].Position = position + vec3(-0.5,  0.5, 0.0) * scale;
}

bool IsParticleDead(uint index)
{
	return particles[index].Lifetime <= 0.0f;
}

void EmitParticle(uint index, Particle particle)
{
	particles[index] = particle;
	SetQuadPosition(index, particle.Position, u_Emitter.InitialScale.x);
	ParticleCount = ParticleCount + 1;
	TimeSinceLastEmit = 0.0f;
}

void UpdateParticle(uint index)
{
	vec2 uv = vec2(u_Emitter.DeltaTime, float(index) / 256.0f);
	float seed = 0.12345;

	vec3 randomVector = vec3(rand(seed, uv) - 0.5f, rand(seed, uv) - 0.5f, rand(seed, uv) - 0.5f);
	vec3 candidate = normalize(randomVector) * 5.0f + u_Emitter.Direction;
//  particles[index].Velocity += candidate * 0.00005f;

	particles[index].Velocity += randomVector * u_Emitter.VelocityRandomness;
	particles[index].Velocity.y -= u_Emitter.Gravity;
	particles[index].Position += particles[index].Velocity * u_Emitter.DeltaTime;
	particles[index].Lifetime -= u_Emitter.DeltaTime;
//	particles[index].Scale.x = particles[index].Lifetime / u_Emitter.InitialLifetime;
	particles[index].Color = vec3(particles[index].Lifetime);
	SetQuadPosition(index, particles[index].Position, particles[index].Scale.x);
}

layout(local_size_x = 1) in;
void main()
{
	uint particleIndex = gl_GlobalInvocationID.x;

	vec2 uv = vec2(u_Emitter.DeltaTime, float(particleIndex) / 256.0f);
	float seed = 0.12345;

	TimeSinceLastEmit += u_Emitter.DeltaTime;

	for (uint i = 0; i < u_Emitter.EmissionRate; i++)
	{
		if (ParticleCount < u_Emitter.MaxParticles && TimeSinceLastEmit >= 1 / u_Emitter.EmissionRate)
		{	
			uint index = i + ParticleCount;

			Particle particle;
			particle.Position = u_Emitter.Position;
			particle.Lifetime = u_Emitter.InitialLifetime * rand(seed, uv);
			particle.Rotation = u_Emitter.InitialRotation;
			particle.Speed = u_Emitter.InitialSpeed;
			particle.Scale = u_Emitter.InitialScale;
			particle.Color = u_Emitter.InitialColor;
			particle.Velocity = (u_Emitter.Direction * u_Emitter.InitialSpeed) + vec3(rand(seed, uv) - 0.5f, rand(seed, uv) - 0.5f, rand(seed, uv) - 0.5f) * u_Emitter.DirectionrRandomness;

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