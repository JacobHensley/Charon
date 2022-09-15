#Shader RayGen
#version 460
#extension GL_EXT_ray_tracing : require

layout(binding = 0) uniform accelerationStructureEXT u_TopLevelAS;

layout (binding = 1, rgba8) uniform image2D o_Image;

layout(binding = 2) uniform CameraBuffer
{
	mat4 ViewProjection;
	mat4 InverseViewProjection;
	mat4 View;
	mat4 InverseView;
	mat4 InverseProjection;
} u_CameraBuffer;

struct RayDesc
{
	vec3 Origin;
	vec3 Direction;
	float TMin, TMax;
};

struct Payload
{
	float Distance;
	vec3 Albedo;
	float Roughness;
	vec3 WorldPosition;
	vec3 WorldNormal;
};

layout(location = 0) rayPayloadEXT Payload g_RayPayload;

vec3 TracePath(RayDesc ray)
{
	uint flags = gl_RayFlagsOpaqueEXT;
	uint mask = 0xff;

	vec3 color = vec3(0.0);
	vec3 throughput = vec3(1.0);
	const int MAX_BOUNCES = 5; 

	float numPaths = 0.0f;

	for (int bounceIndex = 0; bounceIndex < MAX_BOUNCES; bounceIndex++)
	{
		traceRayEXT(u_TopLevelAS, flags, mask, 0, 0, 0, ray.Origin, ray.TMin, ray.Direction, ray.TMax, 0);

		Payload payload = g_RayPayload;
		numPaths += 1.0;

		// Miss
		if (payload.Distance == -1.0)
		{
			vec3 clearColor = vec3(0, 0, 0);
			color += clearColor;
			break;
		}

		vec3 diffuse = payload.Albedo; // Do direct lighting here
		color += diffuse * throughput;
		if (payload.Roughness == 1.0)
		{
			break;
		}

		//numPaths += 1.0;
		// color += payload.WorldNormal;

		// Cast reflection ray
		ray.Origin = payload.WorldPosition;
		ray.Origin +=  payload.WorldNormal * 0.0001 - normalize(ray.Direction) * 0.0001;
		ray.Direction = reflect(ray.Direction, payload.WorldNormal);

		float maxAlbedo = 0.8;
		throughput *= min(payload.Albedo, vec3(maxAlbedo));
	}
	
	return color / numPaths;
}

void main()
{
	vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
	vec2 inUV = pixelCenter / vec2(gl_LaunchSizeEXT.xy);
	vec2 d = inUV * 2.0 - 1.0;

	vec4 target = u_CameraBuffer.InverseProjection * vec4(d.x, d.y, 1, 1);
	vec4 direction = u_CameraBuffer.InverseView * vec4(normalize(target.xyz / target.w), 0); // World space

	RayDesc desc;
	desc.Origin = u_CameraBuffer.InverseView[3].xyz; // Camera position
	desc.Direction = direction.xyz; //
	desc.TMin = 0.0;
	desc.TMax = 1e27f;

	vec3 color = vec3(0.0);

	int samples = 1;
	for (int i = 0; i < samples; i++)
	{
		color += TracePath(desc);
	}

	imageStore(o_Image, ivec2(gl_LaunchIDEXT.xy), vec4(color, 1));

#if 0
	// gl_LaunchIDEXT <- pixel index

	/*
	// as, flags, cullmask, offset x3, origin, min, dir, max, unknown
	traceRayEXT(u_TopLevelAS, flags, mask, 0, 0, 0, desc.Origin, desc.TMin, desc.Direction, desc.TMax, 0);

	vec3 color = vec3((1.0 - (g_RayPayload.Distance * 0.1)) * 0.5);
	color = g_RayPayload.Albedo;
	*/

	if (g_RayPayload.Distance == -1.0)
	{
		vec3 clearColor = vec3(1, 0, 1);
		imageStore(o_Image, ivec2(gl_LaunchIDEXT.xy), vec4(clearColor, 1.0));
		imageStore(o_Image, ivec2(gl_LaunchIDEXT.xy), vec4(0, 0, 0, 1.0));
	}
	else
	{
		imageStore(o_Image, ivec2(gl_LaunchIDEXT.xy), vec4(color, 1));
	}

	// 
#endif
}