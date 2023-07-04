#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inFragPos;
layout(location = 3) in vec2 inTextureCoords;
layout(location = 4) in vec4 inShadowCoords;

struct CameraData {
	mat4 view;
	mat4 projection;
	vec3 pos;
};

layout(set = 0, binding = 0) uniform SceneData {
	CameraData cameraData;
	vec4 ambientColor;
	vec4 fogColor;
	vec4 fogDistances;
	vec4 sunlightDirection;
	vec4 sunlightColor;
	mat4 sunlightShadowMat;
} sceneData;

layout(set = 0, binding = 1) uniform sampler2D shadowSampler;

layout(set = 2, binding = 0) uniform sampler2D tex;

float CalcShadow(vec4 fragPos) {
	vec3 projCoords = fragPos.xyz / fragPos.w;
	projCoords.xy = projCoords.xy * 0.5 + 0.5;

	if (projCoords.z >= 1) return 1;

	vec3 lightDir = normalize(-sceneData.sunlightDirection.xyz);
	float bias = max(0.0001, 0.001 * (1.0 - dot(inNormal, lightDir)));
	float shadow = 0.0;
	vec2 texelSize = 1.0 / textureSize(shadowSampler, 0);

	float currentDepth = projCoords.z - bias;

	for(int x = -1; x <= 1; x++) {
		for(int y = -1; y <= 1; y++) {
			float pcfDepth = texture(shadowSampler, projCoords.xy + vec2(x, y) * texelSize).r;
			shadow += currentDepth > pcfDepth ? 1.0 : 0.0;
		}
	}
	shadow /= 9;

	return 1 - shadow;
}

void main() {
	vec3 tex_color = texture(tex, inTextureCoords).xyz;

	vec3 ambient = sceneData.ambientColor.xyz * sceneData.ambientColor.w;

	vec3 lightDir = -sceneData.sunlightDirection.xyz;
	float lightAngle = clamp(dot(inNormal, lightDir), 0.0, 1.0);

	float shadow = 0.f;
	if (lightAngle > 0.01) shadow = CalcShadow(inShadowCoords);

	vec3 diffuse = lightAngle * sceneData.sunlightColor.xyz;

	vec3 viewDir = normalize(sceneData.cameraData.pos - inFragPos);
	vec3 halfwayDir = normalize(lightDir + viewDir);
	float spec = pow(max(0.0, dot(inNormal, halfwayDir)), 32);
	vec3 specular = spec * sceneData.sunlightColor.xyz * sceneData.sunlightColor.w;

	vec3 color = tex_color *  (ambient + shadow * (diffuse + specular));
	outColor = vec4(color, 1.f);
}