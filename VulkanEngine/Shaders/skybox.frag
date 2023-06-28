#version 460

layout(location = 0) in vec3 inTextureCoords;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform samplerCube skybox;

void main() {
	outColor = texture(skybox, inTextureCoords);
}