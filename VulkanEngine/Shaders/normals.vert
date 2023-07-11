#version 460

layout(location = 0) out VS_OUT {
	vec3 normal;
} vs_out;

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 color;
layout(location = 3) in vec2 textureCoords;

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
	gl_Position = modelMatrix * vec4(pos, 1.f);
	mat3 normalMat = mat3(objectBuffer.objects[index].normalMat);
	vs_out.normal = normalize(normalMat * normal);
}