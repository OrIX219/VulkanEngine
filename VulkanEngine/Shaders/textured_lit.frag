#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 textureCoords;

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

layout(set = 2, binding = 0) uniform sampler2D tex;

void main() {
	vec3 color = texture(tex, textureCoords).xyz * sceneData.ambientColor.xyz;
	outColor = vec4(color, 1.f);
}