#Shader ClosestHit
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

struct Payload
{
	float Distance;
	vec3 Albedo;
	float Roughness;
	vec3 WorldPosition;
	vec3 WorldNormal;
};

layout(location = 0) rayPayloadInEXT Payload g_RayPayload;

hitAttributeEXT vec2 g_HitAttributes;

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

Vertex InterpolateVertex(Vertex vertices[3], vec3 barycentrics)
{
	Vertex vertex;
	vertex.Position = vec3(0.0);
	vertex.Normal = vec3(0.0);
	vertex.Tangent = vec3(0.0);
	vertex.TextureCoords = vec2(0.0);
	
	for (uint i = 0; i < 3; i++)
	{
		vertex.Position += vertices[i].Position * barycentrics[i];
		vertex.Normal += vertices[i].Normal * barycentrics[i];
		vertex.Tangent += vertices[i].Tangent * barycentrics[i];
		vertex.TextureCoords += vertices[i].TextureCoords * barycentrics[i];
	}

	vertex.Normal = normalize(vertex.Normal);
	vertex.Tangent = normalize(vertex.Tangent);

	return vertex;	
}

void main()
{
	uint bufferIndex = m_SubmeshData.Data[gl_InstanceCustomIndexEXT * 4 + 0];
	uint vertexOffset = m_SubmeshData.Data[gl_InstanceCustomIndexEXT * 4 + 1];
	uint indexOffset = m_SubmeshData.Data[gl_InstanceCustomIndexEXT * 4 + 2];
	uint materialIndex  = m_SubmeshData.Data[gl_InstanceCustomIndexEXT * 4 + 3];

	uint index0 = m_IndexBuffers[bufferIndex].Data[gl_PrimitiveID * 3 + 0 + indexOffset];
	uint index1 = m_IndexBuffers[bufferIndex].Data[gl_PrimitiveID * 3 + 1 + indexOffset];
	uint index2 = m_IndexBuffers[bufferIndex].Data[gl_PrimitiveID * 3 + 2 + indexOffset];
	
	Vertex vertices[3] = Vertex[](
		UnpackVertex(bufferIndex, index0, vertexOffset),
		UnpackVertex(bufferIndex, index1, vertexOffset),
		UnpackVertex(bufferIndex, index2, vertexOffset)
	);

	// Weight to each vertex
	vec3 barycentrics = vec3(1.0 - g_HitAttributes.x - g_HitAttributes.y, g_HitAttributes.x, g_HitAttributes.y);
	Vertex vertex = InterpolateVertex(vertices, barycentrics);

	vec3 worldPosition = gl_ObjectToWorldEXT * vec4(vertex.Position, 1.0);
	vec3 worldNormal = normalize(mat3(gl_ObjectToWorldEXT) * vertex.Normal);

	g_RayPayload.Distance = gl_RayTmaxEXT;
	g_RayPayload.Albedo = vertex.Normal * 0.5 + 0.5;
	g_RayPayload.Roughness = 0.0;
	g_RayPayload.WorldPosition = worldPosition;
	g_RayPayload.WorldNormal = worldNormal;

	if (gl_InstanceCustomIndexEXT == 4) // Sphere
	{
		g_RayPayload.Albedo = vec3(0.5);
		g_RayPayload.Roughness = 0.0;
	}

}