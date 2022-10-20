#Shader RayGen
#version 460
#extension GL_EXT_ray_tracing : require

layout(binding = 0) uniform accelerationStructureEXT u_TopLevelAS;

layout (binding = 1, rgba8) uniform image2D o_Image;
layout (binding = 2, rgba32f) uniform image2D o_AccumulationImage;

layout(binding = 3) uniform CameraBuffer
{
	mat4 ViewProjection;
	mat4 InverseViewProjection;
	mat4 View;
	mat4 InverseView;
	mat4 InverseProjection;
} u_CameraBuffer;


layout(binding = 7) uniform Scene
{
	vec3 DirectionalLight_Direction;
	vec3 PointLight_Position;
	uint FrameIndex;
} u_Scene;

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

const float PI = 3.14159265359;

uint WangHash(uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

uint Xorshift(uint seed)
{
    // Xorshift algorithm from George Marsaglia's paper
    seed ^= (seed << 13);
    seed ^= (seed >> 17);
    seed ^= (seed << 5);
    return seed;
}

float GetRandomNumber(inout uint seed)
{
	seed = WangHash(seed);
	return float(Xorshift(seed)) * (1.0 / 4294967296.0);
}

vec3 GetRandomCosineDirectionOnHemisphere(vec3 direction, inout uint seed)
{
	float a = GetRandomNumber(seed) * (PI * 2.0f);
	float z = GetRandomNumber(seed) * 2.0 - 1.0;
	float r = sqrt(1.0 - z * z);

	vec3 p = vec3(r * cos(a), r * sin(a), z) + direction;
	return normalize(p);
}

float LightVisibility(Payload payload, vec3 lightVector, float maxValue)
{
	RayDesc ray;
	ray.Origin = payload.WorldPosition + (payload.WorldNormal * 0.001);
	ray.Direction = lightVector;
	ray.TMin = 0.0;
	ray.TMax = maxValue;

	uint mask = 0xff;
	uint flags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT | gl_RayFlagsOpaqueEXT;

	g_RayPayload.Distance = 0.0;
	traceRayEXT(u_TopLevelAS, flags, mask, 0, 0, 0, ray.Origin, ray.TMin, ray.Direction, ray.TMax, 0);

	return (g_RayPayload.Distance < 0.0) ? 1.0 : 0.0;
}

vec3 DirectionalLight(Payload payload)
{
	vec3 toLight = -u_Scene.DirectionalLight_Direction;
	float intensity = max(dot(payload.WorldNormal, toLight), 0.0);
	float visibility = LightVisibility(payload, toLight, 1e27f);
	return payload.Albedo * intensity * visibility;
}

vec3 PointLight(Payload payload)
{
	vec3 toLight = u_Scene.PointLight_Position - payload.WorldPosition;
	float dist = length(toLight);
	vec3 lightDir = normalize(toLight);
	float range = 5.5;

	float falloff = max((range-dist)/range, 0.0);
	falloff = falloff * falloff;

	float angle = max(dot(payload.WorldNormal, lightDir), 0.0);
	float intensity = angle * falloff;
	float visibility = LightVisibility(payload, lightDir, dist);
	return payload.Albedo * intensity * visibility;
}

vec3 DiffuseLighting(Payload payload)
{
	vec3 directionalLight = DirectionalLight(payload);
	vec3 pointLight = PointLight(payload);

	//return pointLight;
	return directionalLight + pointLight;
}

vec3 RandomPointInUnitCircle(inout uint seed)
{
	while (true) 
	{
		vec3 p = vec3(GetRandomNumber(seed) * 2.0 - 1.0, GetRandomNumber(seed) * 2.0 - 1.0, GetRandomNumber(seed) * 2.0 - 1.0);
		if ((p.length() * p.length()) < 1) continue;
		return p;
	}
}

vec3 TracePath(RayDesc ray, uint seed)
{
	uint flags = gl_RayFlagsOpaqueEXT;
	uint mask = 0xff;

	vec3 color = vec3(0.0);
	vec3 throughput = vec3(1.0);
	const int MAX_BOUNCES = 20;

	float numPaths = 0.0f;
	bool twoSided = false;

	for (int bounceIndex = 0; bounceIndex < MAX_BOUNCES; bounceIndex++)
	{
		traceRayEXT(u_TopLevelAS, flags, mask, 0, 0, 0, ray.Origin, ray.TMin, ray.Direction, ray.TMax, 0);

		Payload payload = g_RayPayload;

		bool backFace = dot(normalize(ray.Direction), payload.WorldNormal) > 0.0;
		if (backFace)
		{
			if (!twoSided)
			{
				ray.Origin = payload.WorldPosition;
				ray.Origin += ray.Direction * 0.0001;
				bounceIndex--;
				continue;
			}

			if (twoSided)
				payload.WorldNormal = -payload.WorldNormal;
		}

		numPaths += ((throughput.r + throughput.g + throughput.b) / 3);

		// Miss
		if (payload.Distance == -1.0)
		{
			vec3 clearColor = vec3(0.4, 0.6, 0.8);
			clearColor = vec3(0.0);
			color += clearColor * throughput;
			break;
		}

		vec3 diffuse = DiffuseLighting(payload); // Do direct lighting here
		color += diffuse * throughput;

		seed++;

		//numPaths += 1.0;
		// color += payload.WorldNormal;
		
		// Cast reflection ray
		ray.Origin = payload.WorldPosition;
		ray.Origin +=  payload.WorldNormal * 0.0001 - ray.Direction * 0.0001;

		if (payload.Roughness == 0.0f)
		{

			ray.Direction = reflect(ray.Direction, payload.WorldNormal) + RandomPointInUnitCircle(seed) * 0.001f;
		}
		else
		{
			ray.Direction = GetRandomCosineDirectionOnHemisphere(payload.WorldNormal, seed);
		}

		float maxAlbedo = 0.9;
		throughput *= min(payload.Albedo, vec3(maxAlbedo));

		float minEnergy = 0.005;
		if (throughput.r < minEnergy && throughput.g < minEnergy && throughput.b < minEnergy)
			break;
	}
	
	return color;
}

void main()
{
	uint frameNumber = u_Scene.FrameIndex;
	uint seed = gl_LaunchIDEXT.x + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x;
	seed *= frameNumber;

	vec3 color = vec3(0.0);

	int samples = 2;
	for (int i = 0; i < samples; i++)
	{
		vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
		//if (i > 0)
		{
			vec2 offsets = vec2(GetRandomNumber(seed), GetRandomNumber(seed)) * 2.0 - 1.0;
			pixelCenter += offsets;
		}

		vec2 inUV = pixelCenter / vec2(gl_LaunchSizeEXT.xy);
		vec2 d = inUV * 2.0 - 1.0;

		vec4 target = u_CameraBuffer.InverseProjection * vec4(d.x, d.y, 1, 1);
		vec4 direction = u_CameraBuffer.InverseView * vec4(normalize(target.xyz / target.w), 0); // World space

		RayDesc desc;
		desc.Origin = u_CameraBuffer.InverseView[3].xyz; // Camera position
		desc.Direction = normalize(direction.xyz); //
		desc.TMin = 0.0;
		desc.TMax = 1e27f;


		color += TracePath(desc, seed);
	}

	float numPaths = samples;
	if (frameNumber > 1)
	{
		vec4 data = imageLoad(o_AccumulationImage, ivec2(gl_LaunchIDEXT.xy));
		vec3 previousColor = data.xyz;
		float numPreviousPaths = data.w;

		color += previousColor;
		numPaths += numPreviousPaths;

		imageStore(o_AccumulationImage, ivec2(gl_LaunchIDEXT.xy), vec4(color, numPaths));
	}
	else
	{
		imageStore(o_AccumulationImage, ivec2(gl_LaunchIDEXT.xy), vec4(0.0));
	}

	color /= numPaths;
	imageStore(o_Image, ivec2(gl_LaunchIDEXT.xy), vec4(color, 1));
}