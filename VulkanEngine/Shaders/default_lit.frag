#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 inColor;

struct CameraData {
	mat4 view;
	mat4 projection;
};

layout(set = 0, binding = 0) uniform SceneData {
	CameraData camera_data;
	vec4 ambientColor;
	vec4 fogColor;
	vec4 fogDistances;
	vec4 sunlightDirection;
	vec4 sunlightColor;
} sceneData;

void main() {
	outColor = vec4(inColor + sceneData.ambientColor.xyz, 1.f);
}