#Shader Miss
#version 460
#extension GL_EXT_ray_tracing : require

struct Payload
{
	float Distance;
};

layout(location = 0) rayPayloadInEXT Payload g_RayPayload;

void main()
{
	g_RayPayload.Distance = -1.0;
}