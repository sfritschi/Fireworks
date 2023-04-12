#version 450 core

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec3 inCol;
layout(location = 2) in vec2 inParticlePos;

layout(location = 0) out vec3 fragCol;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;    
    mat4 view;    
    mat4 proj;    
} ubo;

void main()
{
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPos + inParticlePos, 0.0, 1.0);
    fragCol = inCol;    
}
