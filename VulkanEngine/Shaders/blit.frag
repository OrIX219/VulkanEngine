#version 460

layout(set = 0, binding = 0) uniform sampler2D inputTexture;

layout(location = 0) in vec2 textureCoords;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform constants {
	float exposure;
};

void main() {
	vec3 hdrColor = texture(inputTexture, textureCoords).rgb;
	vec3 mapped = vec3(1.f) - exp(-hdrColor * exposure);
	outColor = vec4(mapped, 1.f);
}