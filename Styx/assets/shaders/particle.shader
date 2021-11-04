#Shader Vertex
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Color;
layout(location = 2) in vec3 a_Velocity; // unused
layout(location = 3) in float RemainingLifetime; // unused

layout(location = 0) out vec3 v_Color;

layout(set = 0, binding = 0) uniform CameraBuffer
{
    mat4 ViewProjection;
    mat4 InverseViewProjection;
} u_CameraBuffer;

void main()
{
    gl_Position = u_CameraBuffer.ViewProjection * vec4(a_Position, 1.0);
    v_Color = a_Color;
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