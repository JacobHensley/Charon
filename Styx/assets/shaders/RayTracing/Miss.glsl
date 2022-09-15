#Shader Miss
#version 460
#extension GL_EXT_ray_tracing : require

struct Payload
{
	float Distance;
	vec3 Albedo;
	float Roughness;
	vec3 WorldPosition;
	vec3 WorldNormal;
};

layout(location = 0) rayPayloadInEXT Payload g_RayPayload;

void main()
{
	g_RayPayload.Distance = -1.0;
	g_RayPayload.Albedo = vec3(0.0);
	g_RayPayload.Roughness = 0.0;
	g_RayPayload.WorldPosition = vec3(0.0);
	g_RayPayload.WorldNormal = vec3(0.0);
}