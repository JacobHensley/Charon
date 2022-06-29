#Shader Compute
#version 450

/////////////////////////////
// Move to include  v
/////////////////////////////

struct Vertex
{
	vec3 Position;
	float Padding;
};

struct Particle
{
	vec3 Position;
	float CurrentLife;
	vec3 Rotation;
	float Speed;
	vec3 Scale;
	float Lifetime;
	vec3 Color;
	vec3 Velocity;
};

layout(std430, binding = 0) buffer ParticleBuffer
{
	Particle particles[];
} u_ParticleBuffer;

layout(std430, binding = 1) buffer AliveBufferPreSimulate
{
	uint Indices[];
} u_AliveBufferPreSimulate;

layout(std430, binding = 2) buffer AliveBufferPostSimulate
{
	uint Indices[];
} u_AliveBufferPostSimulate;

layout(std430, binding = 3) buffer DeadBuffer
{
	uint Indices[];
} u_DeadBuffer;

layout(std140, binding = 4) buffer VertexBuffer
{
	Vertex vertices[];
} u_VertexBuffer;

layout(std140, binding = 5) buffer CounterBuffer
{
	uint AliveCount;
	uint DeadCount;
	uint RealEmitCount;
	uint AliveCount_AfterSimulation;
} u_CounterBuffer;

layout(std140, binding = 6) buffer IndirectDrawBuffer
{
	uvec3 DispatchEmit;
	uvec3 DispatchSimulation;
	uvec4 DrawParticles;
	uint FirstInstance;
} u_IndirectDrawBuffer;

struct GradientPoint
{
	vec3 Color;
	float Position;
};

layout(std140, binding = 7) uniform ParticleEmitter
{
	vec3 InitialRotation;
	float InitialLifetime;
	vec3 InitialScale;
	float InitialSpeed;
	vec3 InitialColor;
	float Gravity;
	vec3 Position;
	uint EmissionQuantity;
	vec3 Direction;
	uint MaxParticles;
	float DirectionRandomness;
	float VelocityRandomness;

	uint GradientPointCount;
	uint Padding0;
	GradientPoint ColorGradientPoints[10];

	uint VelocityCurvePointCount;
	uint Padding1;
	uint Padding2;
	uint Padding3;
	vec4 VelocityCurvePoints[12];

	float Time;
	float DeltaTime;
} u_Emitter;

layout(binding = 8) uniform CameraBuffer
{
	mat4 ViewProjection;
	mat4 InverseViewProjection;
	mat4 View;
	mat4 InverseView;
} u_CameraBuffer;

layout(std430, binding = 9) buffer CameraDistanceBuffer
{
	uint DistanceToCamera[];
} u_CameraDistanceBuffer;

layout(binding = 10) uniform sampler2D u_DepthTexture;

float rand(inout float seed, in vec2 uv)
{
	float result = fract(sin(seed * dot(uv, vec2(12.9898, 78.233))) * 43758.5453);
	seed += 1;
	return result;
}

float LinearizeDepth(float z, float near, float far)
{
	float lin = 2.0 * far * near / (near + far - z * (near - far));
	return lin;
}

// Reconstructs world-space position from depth buffer
//	uv		: screen space coordinate in [0, 1] range
//	z		: depth value at current pixel
//	InvVP	: Inverse of the View-Projection matrix that was used to generate the depth value
vec3 reconstruct_position(in vec2 uv, in float z, in mat4 inverse_view_projection)
{
	float x = uv.x * 2.0 - 1.0;
	float y = (1.0 - uv.y) * 2.0 - 1.0;
	vec4 position_s = vec4(x, y, z, 1.0);
	vec4 position_v = inverse_view_projection * position_s;
	return position_v.xyz / position_v.w;
}

const uint THREADCOUNT_EMIT = 256;
const uint THREADCOUNT_SIMULATION = 256;

/////////////////////////////
// Move to include  ^
/////////////////////////////

vec3 GetParticleColor(float lifePercentage, vec3 particleColor)
{
	if (u_Emitter.GradientPointCount == 0)
		return particleColor;

	// Gradient position == lifePercentage
	float position = clamp(lifePercentage, 0.0f, 1.0f);

	GradientPoint lower;
	lower.Position = -1.0f;
	GradientPoint upper;
	upper.Position = -1.0f;

	// If only 1 point, or we're beneath the first point, return the first point
	if (u_Emitter.GradientPointCount == 1 || position <= u_Emitter.ColorGradientPoints[0].Position)
		return u_Emitter.ColorGradientPoints[0].Color;

	// If we're beyond the last point, return the last point
	if (position >= u_Emitter.ColorGradientPoints[u_Emitter.GradientPointCount - 1].Position)
		return u_Emitter.ColorGradientPoints[u_Emitter.GradientPointCount - 1].Color;

	for (int i = 0; i < u_Emitter.GradientPointCount; i++)
	{
		if (u_Emitter.ColorGradientPoints[i].Position <= position)
		{
			if (lower.Position == -1.0f || lower.Position <= u_Emitter.ColorGradientPoints[i].Position)
				lower = u_Emitter.ColorGradientPoints[i];
		}

		if (u_Emitter.ColorGradientPoints[i].Position >= position)
		{
			if (upper.Position == -1.0f || upper.Position >= u_Emitter.ColorGradientPoints[i].Position)
				upper = u_Emitter.ColorGradientPoints[i];
		}
	}

	float distance = upper.Position - lower.Position;
	float delta = (position - lower.Position) / distance;

	// lerp
	float r = ((1.0f - delta) * lower.Color.r) + ((delta) * upper.Color.r);
	float g = ((1.0f - delta) * lower.Color.g) + ((delta) * upper.Color.g);
	float b = ((1.0f - delta) * lower.Color.b) + ((delta) * upper.Color.b);

	return vec3(r, g, b);
}

vec2 BezierCubicCalc(vec2 p1, vec2 p2, vec2 p3, vec2 p4, float t)
{
	float u = 1.0f - t;
	float w1 = u * u * u;
	float w2 = 3 * u * u * t;
	float w3 = 3 * u * t * t;
	float w4 = t * t * t;
	return vec2(w1 * p1.x + w2 * p2.x + w3 * p3.x + w4 * p4.x, w1 * p1.y + w2 * p2.y + w3 * p3.y + w4 * p4.y);
}

int GetGraphIndex(uint count, float x)
{
	for (int i = 0; i < count; i++)
	{
		vec2 p0 = u_Emitter.VelocityCurvePoints[i].xy;
		vec2 p1 = u_Emitter.VelocityCurvePoints[i+1].zw;
		if (x >= p0.x && x <= p1.x)
			return i;
	}

	return -1;
}

float GetValueFromCuve(uint count, float x)
{
	int graphIndex = GetGraphIndex(count, x);
	if (graphIndex != -1)
	{
		vec2 p0 = u_Emitter.VelocityCurvePoints[graphIndex].xy;
		vec2 p1 = u_Emitter.VelocityCurvePoints[graphIndex + 1].zw;

		vec2 nextPoint = vec2(0.0f, 0.0f);
		if (count > (graphIndex + 4))
			nextPoint = u_Emitter.VelocityCurvePoints[graphIndex + 1].xy;

		// NOTE: keep this?
		float distance = min(abs(p1.x - nextPoint.x), abs(p1.x - p0.x));

		vec2 t0 = p0 + u_Emitter.VelocityCurvePoints[graphIndex].zw * distance;
		vec2 t1 = p1 + u_Emitter.VelocityCurvePoints[graphIndex + 1].xy * distance;

		vec2 pointOnGraph = BezierCubicCalc(p0, t0, t1, p1, (x - p0.x) / (p1.x - p0.x));

		return pointOnGraph.y;
	}

	return 0;
}

layout(local_size_x = THREADCOUNT_SIMULATION) in;
void main()
{
	uint invocationID = gl_GlobalInvocationID.x;

	if (invocationID < u_CounterBuffer.AliveCount)
	{
		uint particleIndex = u_AliveBufferPreSimulate.Indices[invocationID];
		Particle particle = u_ParticleBuffer.particles[particleIndex];
		uint v0 = particleIndex * 4;

		if (particle.CurrentLife > 0)
		{
			// simulate:
			particle.Position += particle.Velocity * u_Emitter.DeltaTime;
			particle.Velocity.y -= u_Emitter.Gravity * u_Emitter.DeltaTime;
			particle.CurrentLife -= u_Emitter.DeltaTime;
			//particle.Color = GetParticleColor(1.0f - particle.CurrentLife / particle.Lifetime, particle.Color);
			
			// particle.Velocity.x = 5.0f;
			// particle.Velocity.y = 0.0f;
			// particle.Velocity.z = 0.0f;
			// particle.Velocity.y = GetValueFromCuve(u_Emitter.VelocityCurvePointCount, 1.0f - particle.CurrentLife / particle.Lifetime) * 10.0f;

			// add to new alive list:
			uint newAliveIndex = atomicAdd(u_CounterBuffer.AliveCount_AfterSimulation, 1);
			u_AliveBufferPostSimulate.Indices[newAliveIndex] = particleIndex;

			// write out vertex:
			u_VertexBuffer.vertices[(v0 + 0)].Position = particle.Position + vec3(-0.5, -0.5, 0.0);
			u_VertexBuffer.vertices[(v0 + 1)].Position = particle.Position + vec3(0.5, -0.5, 0.0);
			u_VertexBuffer.vertices[(v0 + 2)].Position = particle.Position + vec3(0.5, 0.5, 0.0);
			u_VertexBuffer.vertices[(v0 + 3)].Position = particle.Position + vec3(-0.5, 0.5, 0.0);

			// store squared distance to main camera:
			vec3 cameraPosition = u_CameraBuffer.InverseView[3].xyz;
			float particleDist = length(particle.Position - cameraPosition);
			uint distSQ = -uint(particleDist * 1000000.0f);
			u_CameraDistanceBuffer.DistanceToCamera[newAliveIndex] = distSQ;

			// Depth buffer collisions
			{
				vec4 pos2D = u_CameraBuffer.ViewProjection * vec4(particle.Position.xyz, 1);
				pos2D.xyz /= pos2D.w;
				if (pos2D.x > -1 && pos2D.x < 1 && pos2D.y > -1 && pos2D.y < 1)
				{
					vec2 uv = pos2D.xy * vec2(0.5f, -0.5f) + 0.5f;
					vec2 pixel = uv;

					vec2 depthTextureSize = vec2(textureSize(u_DepthTexture, 0));

					float depth0 = texture(u_DepthTexture, pixel).r;

					float surfaceLinearDepth = LinearizeDepth(depth0, 0.1f, 100.0f);
					float surfaceThickness = 5.0f;

					float particleSize = 1;

					// check if particle is colliding with the depth buffer, but not completely behind it:
					if ((pos2D.w + particleSize > surfaceLinearDepth) && (pos2D.w - particleSize < surfaceLinearDepth + surfaceThickness))
					{
						particle.Color = vec3(1.0f, 0.0f, 1.0f);

						// Calculate surface normal and bounce off the particle:
						float depth1 = LinearizeDepth(textureLod(u_DepthTexture, pixel + vec2(1.0 / depthTextureSize.x, 0), 0).r, 0.1f, 100.0f);
						float depth2 = LinearizeDepth(textureLod(u_DepthTexture, pixel + vec2(0, -1.0 / depthTextureSize.y), 0).r, 0.1f, 100.0f);

						vec3 p0 = reconstruct_position(uv, depth0, u_CameraBuffer.InverseViewProjection);
						vec3 p1 = reconstruct_position(uv + vec2(1, 0), depth1, u_CameraBuffer.InverseViewProjection);
						vec3 p2 = reconstruct_position(uv + vec2(0, -1), depth2, u_CameraBuffer.InverseViewProjection);

						vec3 surfaceNormal = normalize(cross(p2 - p0, p1 - p0));

						if (dot(particle.Velocity, surfaceNormal) < 0)
						{
							particle.Velocity = reflect(particle.Velocity, surfaceNormal) * 10.0f;
						}
					}
				}
			}

		}
		else
		{
			// kill:
			uint deadIndex = atomicAdd(u_CounterBuffer.DeadCount, 1);
			u_DeadBuffer.Indices[deadIndex] = particleIndex;

			u_VertexBuffer.vertices[(v0 + 0)].Position = vec3(0);
			u_VertexBuffer.vertices[(v0 + 1)].Position = vec3(0);
			u_VertexBuffer.vertices[(v0 + 2)].Position = vec3(0);
			u_VertexBuffer.vertices[(v0 + 3)].Position = vec3(0);
		}

		// write back simulated particle:
		u_ParticleBuffer.particles[particleIndex] = particle;
	}

}