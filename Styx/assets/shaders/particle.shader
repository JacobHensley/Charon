#Shader Vertex
#version 450

layout(location = 0) in vec3 a_Position;

layout(location = 0) out vec3 v_Color;

struct Particle
{
    vec3 Position;
    float Lifetime;
    vec3 Rotation;
    float Speed;
    vec3 Scale;
    vec3 Color;
    vec3 Velocity;
};

layout(std430, binding = 0) buffer ParticleBuffer
{
    Particle particles[];
};

layout(set = 0, binding = 1) uniform CameraBuffer
{
    mat4 ViewProjection;
    mat4 InverseViewProjection;
    mat4 View;
} u_CameraBuffer;

layout(std430, binding = 2) buffer AliveBufferPostSimulate
{
    uint Indices[];
} u_AliveBufferPostSimulate;

const vec3 c_Vertices[4] = vec3[4](
    vec3(-0.5f, -0.5f, 0.0f),
    vec3( 0.5f, -0.5f, 0.0f),
    vec3( 0.5f,  0.5f, 0.0f),
    vec3(-0.5f,  0.5f, 0.0f)
);

void main()
{
    //gl_Position = u_CameraBuffer.ViewProjection * vec4(a_Position, 1.0);

    uint particleIndex = u_AliveBufferPostSimulate.Indices[gl_VertexIndex / 4];
    Particle particle = particles[particleIndex];
    v_Color = particle.Color;

    gl_Position = u_CameraBuffer.ViewProjection * vec4(particle.Position + c_Vertices[gl_VertexIndex % 4] * particle.Scale, 1.0);
}

#Shader Fragment
#version 450

layout(location = 0) in vec3 v_Color;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor.rgb = v_Color;
    outColor.a = 1.0;
}
