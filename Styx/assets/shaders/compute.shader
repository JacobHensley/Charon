#Shader Compute
#version 450

struct Vertex
{
	vec3 Position;
	vec3 Color; // Normal
	vec3 Velocity;
	float RemainingLifetime;
};

layout(std140, binding = 0) buffer VertexBuffer
{
	Vertex vertices[];
};

layout(std140, binding = 1) uniform ParticleUB
{
	vec3 EmitterPosition;
	vec3 EmitterDirection;
	vec3 EmitterDirectionVariation;
	uint EmissionQuantity; // how many new particles to emit
	uint MaxParticles;
	uint ParticleCount;
	float InitalLifetime;

	// Move to renderer UB
	float Time;
	float DeltaTime;
} u_Particle;

layout(std140, binding = 2) buffer CounterBuffer
{
	uint ParticleCount;
};

vec3 hash31(float p)
{
	vec3 p3 = fract(vec3(p) * vec3(.1031, .1030, .0973));
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.xxy + p3.yzz) * p3.zyx);
}

vec3 RandomVec3()
{
	return hash31(u_Particle.Time + gl_GlobalInvocationID.x);
}

vec3 RandomVec3(float offset)
{
	return hash31(u_Particle.Time + gl_GlobalInvocationID.x + offset);
}

void EmitQuad(uint vertexIndex, vec3 position, vec3 color, vec3 velocity)
{
	vertices[vertexIndex + 0].Position = position + vec3(-0.5, -0.5, 0.0);
	vertices[vertexIndex + 0].Color = color;
	vertices[vertexIndex + 0].Velocity = velocity;
	vertices[vertexIndex + 0].RemainingLifetime = u_Particle.InitalLifetime;

	vertices[vertexIndex + 1].Position = position + vec3(0.5, -0.5, 0.0);
	vertices[vertexIndex + 1].Color = color;
	vertices[vertexIndex + 1].Velocity = velocity;
	vertices[vertexIndex + 1].RemainingLifetime = u_Particle.InitalLifetime;

	vertices[vertexIndex + 2].Position = position + vec3(0.5, 0.5, 0.0);
	vertices[vertexIndex + 2].Color = color;
	vertices[vertexIndex + 2].Velocity = velocity;
	vertices[vertexIndex + 2].RemainingLifetime = u_Particle.InitalLifetime;

	vertices[vertexIndex + 3].Position = position + vec3(-0.5, 0.5, 0.0);
	vertices[vertexIndex + 3].Color = color;
	vertices[vertexIndex + 3].Velocity = velocity;
	vertices[vertexIndex + 3].RemainingLifetime = u_Particle.InitalLifetime;

	ParticleCount = ParticleCount + 1;
}

void EmitParticle(uint index)
{
	EmitQuad(index * 4, u_Particle.EmitterPosition + RandomVec3() - 0.5f, vec3(1.0), RandomVec3(float(index)) * 0.003f);
}

void EmitParticles()
{
	for (uint i = 0; i < u_Particle.EmissionQuantity; i++)
		EmitParticle(i + ParticleCount - u_Particle.EmissionQuantity - 1); // Look, I don't know why this minus 1 is important but it is.
}

void MoveQuad(uint vertexIndex)
{
	vertices[vertexIndex + 0].Position = vertices[vertexIndex + 0].Position + vertices[vertexIndex + 0].Velocity;
	vertices[vertexIndex + 1].Position = vertices[vertexIndex + 1].Position + vertices[vertexIndex + 1].Velocity;
	vertices[vertexIndex + 2].Position = vertices[vertexIndex + 2].Position + vertices[vertexIndex + 2].Velocity;
	vertices[vertexIndex + 3].Position = vertices[vertexIndex + 3].Position + vertices[vertexIndex + 3].Velocity;

	vertices[vertexIndex + 0].RemainingLifetime = vertices[vertexIndex + 0].RemainingLifetime - RandomVec3(float(vertexIndex)).x * 0.001f;
	vertices[vertexIndex + 1].RemainingLifetime = vertices[vertexIndex + 1].RemainingLifetime - RandomVec3(float(vertexIndex)).x * 0.001f;
	vertices[vertexIndex + 2].RemainingLifetime = vertices[vertexIndex + 2].RemainingLifetime - RandomVec3(float(vertexIndex)).x * 0.001f;
	vertices[vertexIndex + 3].RemainingLifetime = vertices[vertexIndex + 3].RemainingLifetime - RandomVec3(float(vertexIndex)).x * 0.001f;

	vertices[vertexIndex + 0].Color = vec3(vertices[vertexIndex + 0].RemainingLifetime);
	vertices[vertexIndex + 1].Color = vec3(vertices[vertexIndex + 1].RemainingLifetime);
	vertices[vertexIndex + 2].Color = vec3(vertices[vertexIndex + 2].RemainingLifetime);
	vertices[vertexIndex + 3].Color = vec3(vertices[vertexIndex + 3].RemainingLifetime);

	vertices[vertexIndex + 0].Color.r = 1.0f;
	vertices[vertexIndex + 1].Color.r = 1.0f;
	vertices[vertexIndex + 2].Color.r = 1.0f;
	vertices[vertexIndex + 3].Color.r = 1.0f;
}

bool IsDead(uint particleIndex)
{
	uint vertexIndex = particleIndex * 4;
	// All vertices should in theory have the same RemainingLifetime but this is just a sanity check
	if (vertices[vertexIndex + 0].RemainingLifetime <= 0.0f || vertices[vertexIndex + 1].RemainingLifetime <= 0.0f || vertices[vertexIndex + 2].RemainingLifetime <= 0.0f || vertices[vertexIndex + 3].RemainingLifetime <= 0.0f)
	{
		return true;
	}

	return false;
}

layout(local_size_x = 1) in;
void main()
{
	uint particleIndex = gl_GlobalInvocationID.x;

	EmitParticles();

	// simulation

	for (uint i = 0; i < ParticleCount; i++)
		MoveQuad(i * 4);

	for (uint i = 0; i < ParticleCount; i++)
	{
		if (IsDead(i))
		{
			uint vertexIndex = i * 4;
			uint lastIndex = (ParticleCount - 1) * 4;

			Vertex particleVertex1 = vertices[vertexIndex + 0];
			Vertex particleVertex2 = vertices[vertexIndex + 1];
			Vertex particleVertex3 = vertices[vertexIndex + 2];
			Vertex particleVertex4 = vertices[vertexIndex + 3];

			vertices[vertexIndex + 0] = vertices[lastIndex + 0];
			vertices[vertexIndex + 1] = vertices[lastIndex + 1];
			vertices[vertexIndex + 2] = vertices[lastIndex + 2];
			vertices[vertexIndex + 3] = vertices[lastIndex + 3];

			vertices[lastIndex + 0] = particleVertex1;
			vertices[lastIndex + 1] = particleVertex2;
			vertices[lastIndex + 2] = particleVertex3;
			vertices[lastIndex + 3] = particleVertex4;

			ParticleCount = ParticleCount - 1;
		}
	}

}
