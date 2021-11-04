#Shader Compute
#version 450

struct Vertex
{
	vec3 Position;
	vec3 Color; // Normal
	vec3 Velocity;
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

	// Move to renderer UB
	float Time;
} u_Particle;

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

	vertices[vertexIndex + 1].Position = position + vec3(0.5, -0.5, 0.0);
	vertices[vertexIndex + 1].Color = color;
	vertices[vertexIndex + 1].Velocity = velocity;

	vertices[vertexIndex + 2].Position = position + vec3(0.5, 0.5, 0.0);
	vertices[vertexIndex + 2].Color = color;
	vertices[vertexIndex + 2].Velocity = velocity;

	vertices[vertexIndex + 3].Position = position + vec3(-0.5, 0.5, 0.0);
	vertices[vertexIndex + 3].Color = color;
	vertices[vertexIndex + 3].Velocity = velocity;
}

void EmitParticle(uint index)
{
	EmitQuad(index * 4, u_Particle.EmitterPosition + RandomVec3() - 0.5f, vec3(1.0), RandomVec3(float(index)) * 0.003f);
}

void EmitParticles()
{
	for (uint i = 0; i < u_Particle.EmissionQuantity; i++)
		EmitParticle(i + (u_Particle.ParticleCount - u_Particle.EmissionQuantity));
}

void MoveQuad(uint vertexIndex)
{
	vertices[vertexIndex + 0].Position = vertices[vertexIndex + 0].Position + vertices[vertexIndex + 0].Velocity;
	vertices[vertexIndex + 1].Position = vertices[vertexIndex + 1].Position + vertices[vertexIndex + 1].Velocity;
	vertices[vertexIndex + 2].Position = vertices[vertexIndex + 2].Position + vertices[vertexIndex + 2].Velocity;
	vertices[vertexIndex + 3].Position = vertices[vertexIndex + 3].Position + vertices[vertexIndex + 3].Velocity;
}

layout(local_size_x = 1) in;
void main()
{
	uint particleIndex = gl_GlobalInvocationID.x;
	EmitParticles();

	// simulation

	for (uint i = 0; i < u_Particle.ParticleCount; i++)
		MoveQuad(i * 4);
}