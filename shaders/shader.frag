#version 450 core

layout(location = 0) in vec4 fragCol;

layout(location = 0) out vec4 outCol;

void main()
{
    outCol = fragCol;
}
