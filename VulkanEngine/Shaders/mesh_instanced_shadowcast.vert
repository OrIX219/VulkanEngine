#version 460

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 color;
layout(location = 3) in vec2 textureCoords;

layout(location = 0) out vec3 outNormal;

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
	mat4 transformMatrix = sceneData.cameraData.projection * sceneData.cameraData.view * modelMatrix;
	gl_Position = transformMatrix * vec4(pos, 1.f);
	outNormal = normalize(normal);
}