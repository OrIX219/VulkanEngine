#version 460

layout(location = 0) in vec3 inNormal;

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

void main() {
	vec3 lightDir = normalize(-sceneData.sunlightDirection.xyz);
	float bias = max(0.0001, 0.001 * (1.0 - dot(inNormal, lightDir)));
	gl_FragDepth = gl_FragCoord.z;
	gl_FragDepth += gl_FrontFacing ? bias : 0.0;
}