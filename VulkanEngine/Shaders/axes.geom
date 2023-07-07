#version 460

layout(points) in;
layout(line_strip,max_vertices = 6) out;

layout(location = 0) out vec3 outColor;

const float magnitude = 0.1;

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
	mat4 view = mat4(mat3(sceneData.cameraData.view));
	mat4 proj = sceneData.cameraData.projection;
	mat4 model = mat4(1);
	model[3] = vec4(0, 0, -1.f, 1.f);
	mat4 MVP = proj * model * view;
	vec4 root_vert = gl_in[0].gl_Position;
	outColor = vec3(1, 0, 0);
	gl_Position = MVP * root_vert;
	EmitVertex();
	gl_Position = MVP * (root_vert + vec4(1, 0, 0, 0) * magnitude);
	EmitVertex();
	EndPrimitive();
	outColor = vec3(0, 1, 0);
	gl_Position = MVP * root_vert;
	EmitVertex();
	gl_Position = MVP * (root_vert + vec4(0, 1, 0, 0) * magnitude);
	EmitVertex();
	EndPrimitive();
	outColor = vec3(0, 0, 1);
	gl_Position = MVP * root_vert;
	EmitVertex();
	gl_Position = MVP * (root_vert + vec4(0, 0, 1, 0) * magnitude);
	EmitVertex();
	EndPrimitive();
}