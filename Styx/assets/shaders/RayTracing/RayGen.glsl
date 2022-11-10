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
	float Metallic;
	vec3 WorldPosition;
	vec3 WorldNormal;
	mat3 WorldNormalMatrix;
	vec3 Tangent;
	vec3 View;
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

vec3 DiffuseLighting(Payload payload, uint seed)
{
	vec3 directionalLight = DirectionalLight(payload);
	vec3 pointLight = PointLight(payload);

	vec3 lightPos = u_Scene.PointLight_Position;

	float areaLightSize = 5.01;
	vec2 random = vec2(GetRandomNumber(seed), GetRandomNumber(seed));
	vec3 randomLightPos = lightPos + areaLightSize * vec3(0.0, random);

	vec3 lightDir = normalize(randomLightPos - payload.WorldPosition);
	float d = length(randomLightPos - payload.WorldPosition);

	float visibility = LightVisibility(payload, lightDir, d);

	return directionalLight;

	return payload.Albedo * visibility * 0.5;

	//return pointLight;
	//return directionalLight + pointLight;
}

vec3 FresnelSchlickRoughness(vec3 F0, float cosTheta, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

// GGX/Towbridge-Reitz normal distribution function.
// Uses Disney's reparametrization of alpha = roughness^2
float NdfGGX(float cosLh, float roughness)
{
	float alpha = roughness * roughness;
	float alphaSq = alpha * alpha;

	float denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
	return alphaSq / (PI * denom * denom);
}

// Single term for separable Schlick-GGX below.
float GaSchlickG1(float cosTheta, float k)
{
	return cosTheta / (cosTheta * (1.0 - k) + k);
}

// Schlick-GGX approximation of geometric attenuation function using Smith's method.
float GaSchlickGGX(float cosLi, float NdotV, float roughness)
{
	float r = roughness + 1.0;
	float k = (r * r) / 8.0; // Epic suggests using this roughness remapping for analytic lights.
	return GaSchlickG1(cosLi, k) * GaSchlickG1(NdotV, k);
}

// Returns next ray direction
vec3 SampleMicrofacetBRDF(RayDesc ray, Payload payload, inout uint seed, out vec3 throughput)
{
	vec3 F0 = mix(vec3(0.04), payload.Albedo, payload.Metallic);

	payload.Roughness = max(0.05, payload.Roughness);

	bool specular = GetRandomNumber(seed) > 0.5;
	if (specular)
	{
		float r2 = payload.Roughness * payload.Roughness;
		float theta = acos(sqrt((1.0 - GetRandomNumber(seed)) / (1.0 + (r2 * r2 - 1.0) * GetRandomNumber(seed))));
		float phi = 2.0 * PI * GetRandomNumber(seed);

		vec3 dir = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));

		vec3 H = payload.WorldNormalMatrix * dir;
		H = payload.WorldNormal + dir * r2;
		H = normalize(H);
		vec3 L = reflect(-payload.View, H);

		float cosLh = clamp(dot(payload.WorldNormal, H), 0.0, 1.0);
		float cosLi = clamp(dot(payload.WorldNormal, L), 0.0, 1.0);
		float NdotV = clamp(dot(payload.WorldNormal, payload.View), 0.0, 1.0);
		float VdotH = clamp(dot(payload.View, H), 0.0, 1.0);


		vec3 F = FresnelSchlickRoughness(F0, max(0.0, clamp(dot(H, payload.View), 0.0, 1.0)), payload.Roughness);
		float D = NdfGGX(cosLh, payload.Roughness);
		float G = GaSchlickGGX(cosLi, NdotV, payload.Roughness);
		//throughput = (F * D * G) / max(0.001, 4.0 * cosLi * NdotV);
		throughput = F * G * VdotH / max(0.0001, cosLi * NdotV);

		throughput *= 2.0;

		//throughput = vec3(NdotV);

		//if (any(greaterThan(throughput, vec3(1.0))))
		//	throughput = vec3(1, 0, 1);

		//throughput = min(throughput, vec3(0.9));
		return L;
	}


	// importance sampling (diffuse)
	float theta = asin(sqrt(GetRandomNumber(seed)));
	float phi = 2.0 * PI * GetRandomNumber(seed);

	vec3 diffuseDir = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));

	// TODO: properly do this for normal maps
	vec3 L = normalize(payload.WorldNormal + diffuseDir);
	vec3 H = normalize(payload.View + L);
	
	vec3 F = FresnelSchlickRoughness(F0, max(0.0, clamp(dot(H, payload.View), 0.0, 1.0)), payload.Roughness);

	vec3 kd = (1.0 - F) * (1.0 - payload.Metallic);
	throughput = kd * payload.Albedo;
	throughput *= 2.0;

	return L;
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
	const int MAX_BOUNCES = 10;

	float numPaths = 0.0f;
	bool twoSided = false;

	vec3 contribution = vec3(1.0);

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

//		numPaths += ((throughput.r + throughput.g + throughput.b) / 3);

		// Miss
		if (payload.Distance == -1.0)
		{
			vec3 clearColor = vec3(0.4, 0.6, 0.8);
			clearColor = vec3(0.0);
			vec3 ambientLight = vec3(0.8, 0.9, 1.0);

			color += ambientLight * contribution;
			break;
		}

		seed++;

#define NEW 1
#if NEW
		vec3 throughput = vec3(0.0);
		vec3 directLight = DiffuseLighting(payload, seed); // Do direct lighting here
		ray.Direction = SampleMicrofacetBRDF(ray, payload, seed, throughput);
		//color += directLight * contribution;

		contribution *= throughput;
		//color = payload.Tangent;

		//numPaths += 1.0;
		// color += payload.WorldNormal;
#else
		vec3 diffuse = DiffuseLighting(payload, seed); // Do direct lighting here
		color += diffuse * contribution;

		if (false && payload.Roughness == 0.0f)
		{
			ray.Direction = reflect(ray.Direction, payload.WorldNormal) + RandomPointInUnitCircle(seed) * 0.05f;
		}
		else
		{
			ray.Direction = GetRandomCosineDirectionOnHemisphere(payload.WorldNormal, seed);
		}

		float maxAlbedo = 0.9;
		contribution *= min(payload.Albedo, vec3(maxAlbedo));

		float minEnergy = 0.005;
		if (contribution.r < minEnergy && contribution.g < minEnergy && contribution.b < minEnergy)
			break;
#endif

		// Cast reflection ray
		ray.Origin = payload.WorldPosition;
		ray.Origin += payload.WorldNormal * 0.0001 - ray.Direction * 0.0001;
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