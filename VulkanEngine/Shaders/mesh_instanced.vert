#version 460

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outFragPos;
layout(location = 3) out vec2 outTextureCoords;
layout(location = 4) out vec4 outWorldCoords;

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 color;
layout(location = 3) in vec2 textureCoords;

struct CameraData {
	mat4 view;
	mat4 projection;
	mat4 viewProj;
	vec3 pos;
};

struct DirectionalLight {
	float ambient;
	float diffuse;
	float specular;
	vec3 direction;
	vec4 color;
	mat4 viewProj;
};

struct PointLight {
	float ambient;
	float diffuse;
	float specular;
	vec3 position;
	vec4 color;
	mat4 viewProj[6];
	float constant;
	float linear;
	float quadratic;
	float farPlane;
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
	PointLight pointLights[8];
	uint spotLightsCount;
	SpotLight spotLights[8];
} sceneData;

struct ObjectData {
	mat4 model;
	mat4 normalMat;
	vec4 bounds;
	vec4 extents;
};

layout(std140, set = 1, binding = 0) readonly buffer ObjectBuffer {
	ObjectData objects[];
} objectBuffer;

layout(set = 1, binding = 1) readonly buffer InstanceBuffer {
	uint ids[];
} instanceBuffer;

void main() {
	uint index = instanceBuffer.ids[gl_InstanceIndex];
	mat4 modelMatrix = objectBuffer.objects[index].model;
	mat4 transformMatrix = sceneData.cameraData.viewProj * modelMatrix;
	gl_Position = transformMatrix * vec4(pos, 1.f);
	outColor = color;
	mat3 normalMat = mat3(objectBuffer.objects[index].normalMat);
	outNormal = normalize(normalMat * normal);
	outFragPos = vec3(modelMatrix * vec4(pos, 1.f));
	outTextureCoords = textureCoords;

	outWorldCoords = modelMatrix * vec4(pos, 1.f);
}