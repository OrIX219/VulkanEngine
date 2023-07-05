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
	vec4 direction;
	vec4 color;
	mat4 view;
	mat4 projection;
};

struct PointLight {
  vec3 position;
  vec4 color;
  float constant;
  float linear;
  float quadratic;
};

layout(set = 0, binding = 0) uniform SceneData {
	CameraData cameraData;
	vec4 ambientColor;
	vec4 fogColor;
	vec4 fogDistances;
	DirectionalLight sunlight;
	uint pointLightsCount;
	PointLight pointLights[16];
} sceneData;

void main() {
	gl_Position = sceneData.cameraData.projection * sceneData.cameraData.view * model * vec4(pos, 1.f);
	outColor = color;
	outNormal = normalize(normal);
	outTextureCoords = textureCoords;
}