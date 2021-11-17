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

layout(std140, binding = 0) buffer ParticleBuffer
{
    Particle particles[];
};

layout(set = 0, binding = 1) uniform CameraBuffer
{
    mat4 ViewProjection;
    mat4 InverseViewProjection;
    mat4 View;
} u_CameraBuffer;

void main()
{
    gl_Position = u_CameraBuffer.ViewProjection * vec4(a_Position, 1.0);
    int vertex = int(gl_VertexIndex * 0.25f);
    v_Color = particles[vertex].Color;
//    v_Color = vec3(1.0f, 1.0f, 1.0f);
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
