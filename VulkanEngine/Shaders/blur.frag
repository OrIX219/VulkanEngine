#version 460

layout(location = 0) in vec2 textureCoords;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D inputTexture;

layout(push_constant) uniform constants {
	bool horizontal;
	float blurStrength;
};

float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
	vec2 tex_offset = 1.0 / textureSize(inputTexture, 0) ;
	vec3 result = texture(inputTexture, textureCoords).rgb * weight[0];

	for(int i = 1; i < 5; i++) {
		vec2 offset = horizontal ? vec2(tex_offset.x * i, 0) : vec2(0, tex_offset.y * i);
		result += texture(inputTexture, textureCoords + offset).rgb * weight[i] * blurStrength;
		result += texture(inputTexture, textureCoords - offset).rgb * weight[i] * blurStrength;
	}

	outColor = vec4(result, 1.0);
}