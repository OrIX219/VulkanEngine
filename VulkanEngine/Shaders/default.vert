#version 460

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 color;
layout(location = 3) in vec2 textureCoords;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTextureCoords;

layout(push_constant) uniform constants {
	mat4 model;
};

struct CameraData {
	mat4 view;
	mat4 projection;
	vec3 pos;
};

struct DirectionalLight {
	float ambient;
	float diffuse;
	float specular;
	vec3 direction;
	vec4 color;
	mat4 view;
	mat4 projection;
};

struct PointLight {
	float ambient;
	float diffuse;
	float specular;
	vec3 position;
	vec4 color;
	float constant;
	float linear;
	float quadratic;
};

struct SpotLight {
	float ambient;
	float diffuse;
	float specular;
	vec3 position;
	vec3 direction;
	vec4 color;
	float cutOffInner;
	float cutOffOuter;
};

layout(set = 0, binding = 0) uniform SceneData {
	CameraData cameraData;
	vec4 fogColor;
	vec4 fogDistances;
	uint directionalLightsCount;
	DirectionalLight directionalLights[4];
	uint pointLightsCount;
	PointLight pointLights[16];
	uint spotLightsCount;
	SpotLight spotLights[8];
} sceneData;

void main() {
	gl_Position = sceneData.cameraData.projection * sceneData.cameraData.view * model * vec4(pos, 1.f);
	outColor = color;
	outNormal = normalize(normal);
	outTextureCoords = textureCoords;
}