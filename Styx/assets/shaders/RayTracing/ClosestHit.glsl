#Shader ClosestHit
#version 460
#extension GL_EXT_ray_tracing : require

struct Payload
{
	float Distance;
};

layout(location = 0) rayPayloadInEXT Payload g_RayPayload;

void main()
{
	// gl_ObjectToWorldEXT * vec4();

	g_RayPayload.Distance = gl_RayTmaxEXT;
}