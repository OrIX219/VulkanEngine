#version 460

layout(set = 0, binding = 0) uniform sampler2D inputTexture;
layout(set = 0, binding = 1) uniform sampler2D blurTexture;

layout(location = 0) in vec2 textureCoords;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform constants {
	float gamma;
	float exposure;
};

void main() {
	vec3 hdrColor = texture(inputTexture, textureCoords).rgb;
	vec3 blurColor = texture(blurTexture, textureCoords).rgb;
	hdrColor += blurColor;
	vec3 mapped = vec3(1.f) - exp(-hdrColor * exposure);
	mapped = pow(mapped, vec3(1.0 / gamma));
	outColor = vec4(mapped, 1.f);
}