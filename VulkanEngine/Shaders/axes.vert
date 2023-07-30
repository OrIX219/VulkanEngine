#version 460

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 color;
layout(location = 3) in vec2 textureCoords;
layout(location = 4) in vec4 tangent;

void main() {
	gl_Position = vec4(pos, 1.f);
}