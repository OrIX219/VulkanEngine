#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inFragPos;
layout(location = 3) in vec2 inTextureCoords;

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
} sceneData;

layout(set = 2, binding = 0) uniform sampler2D tex;

void main() {
	vec3 tex_color = texture(tex, inTextureCoords).xyz;

	vec3 ambient = sceneData.ambientColor.xyz * sceneData.ambientColor.w;

	vec3 lightDir = normalize(sceneData.sunlightDirection.xyz);
	float diff = max(0.0, dot(inNormal, lightDir));
	vec3 diffuse = diff * sceneData.sunlightColor.xyz * sceneData.sunlightColor.w;

	vec3 viewDir = normalize(sceneData.cameraData.pos - inFragPos);
	vec3 halfwayDir = normalize(lightDir + viewDir);
	float spec = pow(max(0.0, dot(inNormal, halfwayDir)), 32);
	vec3 specular = spec * sceneData.sunlightColor.xyz * sceneData.sunlightColor.w;

	vec3 color = tex_color *  (ambient + diffuse + specular);
	outColor = vec4(color, 1.f);
}