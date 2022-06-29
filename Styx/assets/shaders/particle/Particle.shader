#Shader Vertex
#version 450

layout(location = 0) in vec3 a_Position;

layout(location = 0) out vec3 v_Color;
layout(location = 1) out float v_Alpha;

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

layout(set = 0, binding = 2) uniform CameraBuffer
{
    mat4 ViewProjection;
    mat4 InverseViewProjection;
    mat4 View;
    mat4 InverseView;
} u_CameraBuffer;

layout(std430, binding = 1) buffer AliveBufferPostSimulate
{
    uint Indices[];
} u_AliveBufferPostSimulate;

layout(std140, binding = 3) uniform ParticleDrawDetails
{
    uint IndexOffset;
} u_ParticleDrawDetails;

vec3 c_Vertices[4] = vec3[4](
    vec3(-0.5f, -0.5f, 0.0f),
    vec3( 0.5f, -0.5f, 0.0f),
    vec3( 0.5f,  0.5f, 0.0f),
    vec3(-0.5f,  0.5f, 0.0f)
);

void main()
{
    uint particleIndex = u_AliveBufferPostSimulate.Indices[gl_VertexIndex / 4];
    Particle particle = particles[particleIndex];
    v_Color = particle.Color;
    v_Alpha = particle.Lifetime / 3.0f;

    vec3 camRightWS = { u_CameraBuffer.View[0][0],  u_CameraBuffer.View[1][0], u_CameraBuffer.View[2][0] }; // X
    vec3 camUpWS = { u_CameraBuffer.View[0][1],  u_CameraBuffer.View[1][1], u_CameraBuffer.View[2][1] };    // Y

    c_Vertices[0] = (camRightWS * -0.5 + camUpWS * -0.5);
    c_Vertices[1] = (camRightWS * 0.5 + camUpWS * -0.5);
    c_Vertices[2] = (camRightWS * 0.5 + camUpWS * 0.5);
    c_Vertices[3] = (camRightWS * -0.5 + camUpWS * 0.5);

    gl_Position = u_CameraBuffer.ViewProjection * vec4(particle.Position + c_Vertices[gl_VertexIndex % 4] * particle.Scale, 1.0);
}

#Shader Fragment
#version 450

layout(location = 0) in vec3 v_Color;
layout(location = 1) in float v_Alpha;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor.rgb = v_Color;
    outColor.a = v_Alpha;
    outColor.a = 1.0;
}
