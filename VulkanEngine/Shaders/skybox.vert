#version 460

layout(location = 0) out vec3 outTextureCoords;

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

layout(set = 0, binding = 0) uniform SceneData {
	CameraData cameraData;
} sceneData;

void main() {
	outTextureCoords = pos;

	mat4 transformMatrix = sceneData.cameraData.projection * mat4(mat3(sceneData.cameraData.view));
	vec4 position = transformMatrix * vec4(pos, 1.0);
	gl_Position = position.xyww;
}