#Shader Compute
#version 450

struct Vertex
{
	vec3 Position;
	vec3 Color; // Normal
};

layout(std140, binding = 0) buffer VertexBuffer
{
	Vertex vertices[];
};

layout(local_size_x = 4) in;
void main()
{
	uint particleIndex = gl_GlobalInvocationID.x;
	uint vertexIndex = particleIndex * 4;

	vec3 particlePostion = vec3(0.0f, 0.0f, 0.0f);
	vec3 particleColor = vec3(1.0, 1.0f, 1.0f);

	if (particleIndex == 0)
	{
		particlePostion = vec3(-2.0, 1.0, 0.0);
		particleColor = vec3(1.0, 0.0, 1.0);
	}
	else if (particleIndex == 1)
	{
		particlePostion = vec3(5.0, 1.0, 0.0);
		particleColor = vec3(0.0, 1.0, 0.0);
	}
	else if (particleIndex == 2)
	{
		particlePostion = vec3(-3.0, 2.0, 2.0);
		particleColor = vec3(1.0, 0.0, 0.0);
	}
	else if (particleIndex == 3)
	{
		particlePostion = vec3(-5.0, 1.0, 0.0);
		particleColor = vec3(0.0, 0.0, 1.0);
	}

	vertices[vertexIndex + 0].Position = particlePostion + vec3(-0.5, -0.5, 0.0);
	vertices[vertexIndex + 0].Color = particleColor;

	vertices[vertexIndex + 1].Position = particlePostion + vec3(0.5, -0.5, 0.0);
	vertices[vertexIndex + 1].Color = particleColor;

	vertices[vertexIndex + 2].Position = particlePostion + vec3(0.5, 0.5, 0.0);
	vertices[vertexIndex + 2].Color = particleColor;

	vertices[vertexIndex + 3].Position = particlePostion + vec3(-0.5, 0.5, 0.0);
	vertices[vertexIndex + 3].Color = particleColor;
}