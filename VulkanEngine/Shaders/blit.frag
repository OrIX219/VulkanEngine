#version 460

layout(set = 0, binding = 0) uniform sampler2D inputTexture;

layout(location = 0) in vec2 textureCoords;

layout(location = 0) out vec4 outColor;

void main() {
	outColor = texture(inputTexture, textureCoords);
}