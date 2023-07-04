#version 460

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTextureCoords;

void main() {
	outColor = vec4(inColor, 1.f);
}