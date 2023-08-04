#version 460

#define MAX_DIR_LIGHT 1
#define MAX_POINT_LIGHT 2
#define MAX_SPOT_LIGHT 2

layout(location = 0) out VS_OUT {
	vec3 color;
	vec3 normal;
	vec3 fragPos;
	vec2 textureCoords;
	vec4 worldCoords;
	vec3 tangentViewPos;
	vec3 tangentFragPos;
	vec3 tangentDirLightDirection[MAX_DIR_LIGHT];
	vec3 tangentPointLightPos[MAX_POINT_LIGHT];
	vec3 tangentSpotLightPos[MAX_SPOT_LIGHT];
} vs_out;

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 color;
layout(location = 3) in vec2 textureCoords;
layout(location = 4) in vec4 tangent;

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
	DirectionalLight directionalLights[MAX_DIR_LIGHT];
	uint pointLightsCount;
	PointLight pointLights[MAX_POINT_LIGHT];
	uint spotLightsCount;
	SpotLight spotLights[MAX_SPOT_LIGHT];
} sceneData;

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
	mat4 transformMatrix = sceneData.cameraData.viewProj * modelMatrix;
	gl_Position = transformMatrix * vec4(pos, 1.f);
	vs_out.color = color;
	mat3 normalMat = mat3(objectBuffer.objects[index].normalMat);
	vs_out.normal = normalize(normalMat * normal);
	vs_out.fragPos = vec3(modelMatrix * vec4(pos, 1.f));
	vs_out.textureCoords = textureCoords;

	vs_out.worldCoords = modelMatrix * vec4(pos, 1.f);

	vec3 T = normalize(normalMat * tangent.xyz);
	vec3 N = vs_out.normal;
	T = normalize(T - dot(T, N) * N);
	vec3 B = normalize(cross(N, T) * tangent.w);
	mat3 TBN = transpose(mat3(T, B, N));
	vs_out.tangentViewPos = TBN * sceneData.cameraData.pos;
	vs_out.tangentFragPos = TBN * vec3(modelMatrix * vec4(pos, 1.f));

  for (int i = 0; i < sceneData.directionalLightsCount; i++)
    vs_out.tangentDirLightDirection[i] = TBN * -sceneData.directionalLights[i].direction;
  for (int i = 0; i < sceneData.pointLightsCount; i++)
    vs_out.tangentPointLightPos[i] = TBN * sceneData.pointLights[i].position;
  for (int i = 0; i < sceneData.spotLightsCount; i++)
      vs_out.tangentSpotLightPos[i] = TBN * sceneData.spotLights[i].position;
}