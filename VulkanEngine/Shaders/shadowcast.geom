#version 460

layout(triangles) in;
layout(triangle_strip, max_vertices = 12) out;

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

void main() {
	for(int i = 0; i < sceneData.directionalLightsCount; i++) {
		gl_Layer = i;
		for(int j = 0; j < 3; j++) {
			gl_Position = sceneData.directionalLights[i].viewProj * gl_in[j].gl_Position;
			EmitVertex();
		}
		EndPrimitive();
	}
}