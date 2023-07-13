#version 460

layout(triangles) in;
layout(triangle_strip, max_vertices = 144) out;

layout(location = 0) out vec4 outFragPos;
layout(location = 1) out uint outLightIdx;

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
	DirectionalLight directionalLights[1];
	uint pointLightsCount;
	PointLight pointLights[2];
	uint spotLightsCount;
	SpotLight spotLights[2];
} sceneData;

void main() {
	for (int i = 0; i < sceneData.pointLightsCount; i++) {
		outLightIdx = i;
		for(int face = 0; face < 6; face++) {
			gl_Layer = 6 * i + face;
			for(int j = 0; j < 3; j++) {
				outFragPos = gl_in[j].gl_Position;
				gl_Position = sceneData.pointLights[i].viewProj[face] * outFragPos;
				EmitVertex();
			}
			EndPrimitive();
		}
	}
}