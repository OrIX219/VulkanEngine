#version 460

layout(location = 0) in vec3 inNormal;

struct CameraData {
	mat4 view;
	mat4 projection;
	vec3 pos;
};

struct DirectionalLight {
	float ambient;
	float diffuse;
	float specular;
	vec3 direction;
	vec4 color;
	mat4 view;
	mat4 projection;
};

struct PointLight {
	float ambient;
	float diffuse;
	float specular;
	vec3 position;
	vec4 color;
	float constant;
	float linear;
	float quadratic;
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
	PointLight pointLights[16];
	uint spotLightsCount;
	SpotLight spotLights[8];
} sceneData;

void main() {
	vec3 lightDir = normalize(-sceneData.directionalLights[0].direction.xyz);
	float bias = max(0.0001, 0.001 * (1.0 - dot(inNormal, lightDir)));
	gl_FragDepth = gl_FragCoord.z;
	gl_FragDepth += gl_FrontFacing ? bias : 0.0;
}