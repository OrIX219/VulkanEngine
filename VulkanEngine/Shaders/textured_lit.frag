#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inFragPos;
layout(location = 3) in vec2 inTextureCoords;
layout(location = 4) in vec4 inShadowCoords;

struct CameraData {
	mat4 view;
	mat4 projection;
	vec3 pos;
};

struct DirectionalLight {
	vec4 direction;
	vec4 color;
	mat4 view;
	mat4 projection;
};

struct PointLight {
  vec3 position;
  vec4 color;
  float constant;
  float linear;
  float quadratic;
};

layout(set = 0, binding = 0) uniform SceneData {
	CameraData cameraData;
	vec4 ambientColor;
	vec4 fogColor;
	vec4 fogDistances;
	DirectionalLight sunlight;
	uint pointLightsCount;
	PointLight pointLights[16];
} sceneData;

layout(set = 0, binding = 1) uniform sampler2D shadowSampler;

layout(set = 2, binding = 0) uniform sampler2D tex;

float CalcShadow(vec4 fragPos) {
	vec3 projCoords = fragPos.xyz / fragPos.w;
	projCoords.xy = projCoords.xy * 0.5 + 0.5;

	if (projCoords.z >= 1) return 1;

	vec3 lightDir = normalize(-sceneData.sunlight.direction.xyz);
	float bias = max(0.0001, 0.001 * (1.0 - dot(inNormal, lightDir)));
	float shadow = 0.0;
	vec2 texelSize = 1.0 / textureSize(shadowSampler, 0);

	float currentDepth = projCoords.z - bias;

	for(int x = -1; x <= 1; x++) {
		for(int y = -1; y <= 1; y++) {
			float pcfDepth = texture(shadowSampler, projCoords.xy + vec2(x, y) * texelSize).r;
			shadow += currentDepth > pcfDepth ? 1.0 : 0.0;
		}
	}
	shadow /= 9;

	return 1 - shadow;
}

vec3 CalcDirectional() {
	vec3 lightDir = normalize(-sceneData.sunlight.direction.xyz);
	float lightAngle = clamp(dot(inNormal, lightDir), 0.0, 1.0);
	vec3 lightColor = sceneData.sunlight.color.xyz * sceneData.sunlight.color.w;

	float shadow = 0.f;
	if (lightAngle > 0.01) shadow = CalcShadow(inShadowCoords);

	vec3 diffuse = lightAngle * lightColor;

	vec3 viewDir = normalize(sceneData.cameraData.pos - inFragPos);
	vec3 halfwayDir = normalize(lightDir + viewDir);
	float spec = pow(max(0.0, dot(inNormal, halfwayDir)), 32);
	vec3 specular = spec * lightColor;

	return (shadow * (diffuse + specular));
}

vec3 CalcPoint() {
	vec3 result = vec3(0);
	for(int i = 0; i < sceneData.pointLightsCount; i++) {
		PointLight light = sceneData.pointLights[i];
		vec3 lightColor = light.color.xyz * light.color.w;

		vec3 lightDir = normalize(light.position - inFragPos);
		float lightAngle = clamp(dot(inNormal, lightDir), 0.0, 1.0);
		vec3 diff = lightAngle * lightColor;

		vec3 viewDir = normalize(sceneData.cameraData.pos - inFragPos);
		vec3 halfwayDir = normalize(lightDir + viewDir);
		float spec = pow(max(0.0, dot(inNormal, halfwayDir)), 32);

		float dist = distance(light.position, inFragPos);
		float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * pow(dist, 2));

		vec3 diffuse = diff * attenuation;
		vec3 specular = spec * lightColor * attenuation;
		
		result += (diffuse + specular);
	}
	return result;
}

void main() {
	vec3 tex_color = texture(tex, inTextureCoords).xyz;

	vec3 ambient = sceneData.ambientColor.xyz * sceneData.ambientColor.w;
	vec3 directional = CalcDirectional();
	vec3 point = CalcPoint();

	vec3 color = tex_color * (ambient + directional + point);
	outColor = vec4(color, 1.f);
}