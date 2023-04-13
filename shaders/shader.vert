#version 450 core

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec4 inCol;
layout(location = 2) in vec2 inParticlePos;
layout(location = 3) in float inOrientation;

layout(location = 0) out vec4 fragCol;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;    
    mat4 view;    
    mat4 proj;    
} ubo;

void main()
{
    // Note: Could also directly pass 2 x 2 rotation matrix
    const float cosTheta = cos(inOrientation);
    const float sinTheta = sin(inOrientation);
    const mat2 rotation = mat2(cosTheta, -sinTheta,
                               sinTheta, cosTheta);
    const vec2 rotatedPos = rotation * inPos;
    
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(rotatedPos + inParticlePos, 0.0, 1.0);
    fragCol = inCol;    
}
