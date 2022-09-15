#Shader Vertex
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Tangent;
layout(location = 3) in vec2 a_TexCoord;

layout(location = 0) out vec3 v_WorldPosition;
layout(location = 1) out vec3 v_Normal;

layout(push_constant) uniform constants
{
    mat4 Transform;
} PushConstants;

layout(set = 0, binding = 0) uniform CameraBuffer
{
    mat4 ViewProjection;
    mat4 InverseViewProjection;
} u_CameraBuffer;

void main() 
{
    v_WorldPosition = vec3(PushConstants.Transform * vec4(a_Position, 1.0));
    gl_Position = u_CameraBuffer.ViewProjection * PushConstants.Transform * vec4(a_Position, 1.0);
    v_Normal = a_Normal;
}

#Shader Fragment
#version 450

layout(location = 0) in vec3 v_WorldPosition;
layout(location = 1) in vec3 v_Normal;

layout(location = 0) out vec4 outColor;

void main() 
{
    outColor.rgb = v_Normal * 0.5 + 0.5;
    outColor.rgb = v_WorldPosition;
    outColor.a = 1.0;
}