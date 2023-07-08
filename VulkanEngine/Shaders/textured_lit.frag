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
	PointLight pointLights[16];
	uint spotLightsCount;
	SpotLight spotLights[8];
} sceneData;

// need samplers for all lights
layout(set = 0, binding = 1) uniform sampler2D shadowSampler;
layout(set = 0, binding = 2) uniform samplerCube pointShadowSampler;

layout(set = 2, binding = 0) uniform sampler2D tex;

float CalcShadow(vec4 fragPos, vec3 lightDirection) {
	vec3 projCoords = fragPos.xyz / fragPos.w;
	projCoords.xy = projCoords.xy * 0.5 + 0.5;

	if (projCoords.z >= 1) return 1;

	vec3 lightDir = normalize(-lightDirection);
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

float CalcPointShadow(vec3 fragPos, vec3 lightPos, float farPlane) {
	vec3 fragToLight = fragPos - lightPos;
	vec3 lightDir = normalize(-fragToLight);
	float bias = max(0.01, 0.1 * (1.0 - dot(inNormal, lightDir)));

	float shadow = 0.0;
	int samples = 20;
	float viewDistance = length(sceneData.cameraData.pos - inFragPos);
	float diskRadius = (1.0 + (viewDistance / farPlane)) / 100.0;
	float currentDepth = length(fragToLight) - bias;
	
	vec3 sampleOffsetDirections[20] = vec3[] (
	   vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
	   vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
	   vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
	   vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
	   vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
	);

	for(int i = 0; i < samples; i++) {
		float closestDepth = texture(pointShadowSampler, fragToLight + sampleOffsetDirections[i] * diskRadius).r;
		closestDepth *= farPlane;
		shadow += currentDepth > closestDepth ? 1.0 : 0.0;
	}
	shadow /= float(samples);

	return 1 - shadow;
}

vec3 CalcDirectional() {
	vec3 result = vec3(0.f);
	for(int i = 0; i < sceneData.directionalLightsCount; i++) {
		DirectionalLight light = sceneData.directionalLights[i];
		vec3 lightDir = normalize(-light.direction);
		float lightAngle = clamp(dot(inNormal, lightDir), 0.0, 1.0);
		vec3 lightColor = light.color.xyz * light.color.w;

		float shadow = 0.f;
		if (lightAngle > 0.01) shadow = CalcShadow(inShadowCoords, light.direction);

		vec3 ambient = light.ambient * lightColor;

		vec3 diffuse = light.diffuse * lightAngle * lightColor;

		vec3 viewDir = normalize(sceneData.cameraData.pos - inFragPos);
		vec3 halfwayDir = normalize(lightDir + viewDir);
		vec3 specular = light.specular * pow(max(0.0, dot(inNormal, halfwayDir)), 32) * lightColor;

		result += ambient + shadow * (diffuse + specular);
	}
	return result;
}

vec3 CalcPoint() {
	vec3 result = vec3(0.f);
	for(int i = 0; i < sceneData.pointLightsCount; i++) {
		PointLight light = sceneData.pointLights[i];
		vec3 lightColor = light.color.xyz * light.color.w;
		vec3 lightDir = normalize(light.position - inFragPos);
		float lightAngle = clamp(dot(inNormal, lightDir), 0.0, 1.0);

		float shadow = 0.f;
		if (lightAngle > 0.01) shadow = CalcPointShadow(inFragPos, light.position, light.farPlane);

		vec3 ambient = light.ambient * lightColor;

		vec3 diffuse = light.diffuse * lightAngle * lightColor;

		vec3 viewDir = normalize(sceneData.cameraData.pos - inFragPos);
		vec3 halfwayDir = normalize(lightDir + viewDir);
		vec3 specular = light.specular * pow(max(0.0, dot(inNormal, halfwayDir)), 32) * lightColor;

		float dist = distance(light.position, inFragPos);
		float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * pow(dist, 2));
		
		result += ambient + (diffuse + specular) * attenuation * shadow;
	}
	return result;
}

vec3 CalcSpot() {
	vec3 result = vec3(0.f);
	for(int i = 0; i < sceneData.spotLightsCount; i++) {
		SpotLight light = sceneData.spotLights[i];
		vec3 lightColor = light.color.xyz * light.color.w;
		vec3 lightDir = normalize(light.position - inFragPos);
		float phi = cos(radians(light.cutOffInner));
		float gamma = cos(radians(light.cutOffOuter));
		float theta = dot(lightDir, normalize(-light.direction));
		float intensity = smoothstep(0.0, 1.0, (theta - gamma) / (phi - gamma));

		vec3 ambient = light.ambient * lightColor;

		float lightAngle = clamp(dot(inNormal, lightDir), 0.0, 1.0);
		vec3 diffuse = light.diffuse * lightAngle * lightColor;

		vec3 viewDir = normalize(sceneData.cameraData.pos - inFragPos);
		vec3 halfwayDir = normalize(lightDir + viewDir);
		vec3 specular = light.specular * pow(max(0.0, dot(inNormal, halfwayDir)), 32) * lightColor;

		result += ambient + (diffuse + specular) * intensity;
	}
	return result;
}

void main() {
	vec3 tex_color = texture(tex, inTextureCoords).xyz;

	vec3 directional = CalcDirectional();
	vec3 point = CalcPoint();
	vec3 spot = CalcSpot();

	vec3 color = tex_color * (directional + point + spot);
	outColor = vec4(color, 1.f);
}