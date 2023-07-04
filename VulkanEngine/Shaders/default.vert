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

layout(set = 0, binding = 0) uniform SceneData {
	CameraData cameraData;
	vec4 ambientColor;
	vec4 fogColor;
	vec4 fogDistances;
	vec4 sunlightDirection;
	vec4 sunlightColor;
	mat4 sunlightShadowMat;
} sceneData;

void main() {
	gl_Position = sceneData.cameraData.projection * sceneData.cameraData.view * model * vec4(pos, 1.f);
	outColor = color;
	outNormal = normalize(normal);
	outTextureCoords = textureCoords;
}