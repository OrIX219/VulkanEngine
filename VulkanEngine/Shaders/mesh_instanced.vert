#version 460

layout(location = 0) out VS_OUT {
	vec3 color;
	vec3 normal;
	vec3 fragPos;
	vec2 textureCoords;
	vec4 worldCoords;
} vs_out;

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 color;
layout(location = 3) in vec2 textureCoords;
layout(location = 4) in vec4 tangent;

struct CameraData {
	mat4 view;
	mat4 projection;
	mat4 viewProj;
	vec3 pos;
};

layout(set = 0, binding = 0) uniform SceneData {
	CameraData cameraData;
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
	vs_out.color = color;
	mat3 normalMat = mat3(objectBuffer.objects[index].normalMat);
	vs_out.normal = normalize(normalMat * normal);
	vs_out.fragPos = vec3(modelMatrix * vec4(pos, 1.f));
	vs_out.textureCoords = textureCoords;

	vs_out.worldCoords = modelMatrix * vec4(pos, 1.f);
}