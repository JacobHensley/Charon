#Shader ClosestHit
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

struct Payload
{
	float Distance;
	vec3 Albedo;
};

layout(location = 0) rayPayloadInEXT Payload g_RayPayload;

layout(std430, binding = 3) buffer Vertices { float Data[]; } m_VertexBuffers[];
layout(std430, binding = 4) buffer Indices { uint Data[]; } m_IndexBuffers[];
layout(std430, binding = 5) buffer SubmeshData { uint Data[]; } m_SubmeshData;

struct Vertex
{
	vec3 Position;
	vec3 Normal;
	vec3 Tangent;
	vec2 TextureCoords;
};

Vertex UnpackVertex(uint vertexBufferIndex, uint index, uint vertexOffset)
{
	index += vertexOffset;

	Vertex vertex;

	const int stride = 44;
	const int offset = stride / 4;

	vertex.Position = vec3(
		m_VertexBuffers[vertexBufferIndex].Data[offset * index + 0],
		m_VertexBuffers[vertexBufferIndex].Data[offset * index + 1],
		m_VertexBuffers[vertexBufferIndex].Data[offset * index + 2]
	);

	vertex.Normal = vec3(
		m_VertexBuffers[vertexBufferIndex].Data[offset * index + 3],
		m_VertexBuffers[vertexBufferIndex].Data[offset * index + 4],
		m_VertexBuffers[vertexBufferIndex].Data[offset * index + 5]
	);

	vertex.Normal = normalize(mat3(gl_ObjectToWorldEXT) * vertex.Normal);
	return vertex;
}

void main()
{
	uint bufferIndex = m_SubmeshData.Data[gl_InstanceCustomIndexEXT * 4 + 0];
	uint vertexOffset = m_SubmeshData.Data[gl_InstanceCustomIndexEXT * 4 + 1];
	uint indexOffset = m_SubmeshData.Data[gl_InstanceCustomIndexEXT * 4 + 2];
	uint materialIndex  = m_SubmeshData.Data[gl_InstanceCustomIndexEXT * 4 + 3];

	uint index = m_IndexBuffers[bufferIndex].Data[gl_PrimitiveID * 3 + indexOffset];
	Vertex vertex = UnpackVertex(bufferIndex, index, vertexOffset);

	// gl_ObjectToWorldEXT * vec4();

	g_RayPayload.Distance = gl_RayTmaxEXT;
	g_RayPayload.Albedo = vertex.Normal * 0.5 + 0.5;
}