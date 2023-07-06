#version 460

layout(triangles) in;
layout(line_strip,max_vertices = 6) out;

layout(location = 0) in VS_OUT {
	vec3 normal;
} gs_in[];

const float magnitude = 0.4;

struct CameraData {
	mat4 view;
	mat4 projection;
	vec3 pos;
};

struct DirectionalLight {
	float ambient;
	float diffuse;
	float specular;
	vec4 direction;
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
	DirectionalLight sunlight;
	SpotLight spotlight;
	uint pointLightsCount;
	PointLight pointLights[16];
} sceneData;

void generateLine(int index) {
	mat4 view_proj = sceneData.cameraData.projection * sceneData.cameraData.view;
	gl_Position = view_proj * gl_in[index].gl_Position;
	EmitVertex();
	gl_Position = view_proj *
		(gl_in[index].gl_Position + vec4(gs_in[index].normal, 0) * magnitude);
	EmitVertex();
	EndPrimitive();
}

void main() {
	generateLine(0);
	generateLine(1);
	generateLine(2);
}