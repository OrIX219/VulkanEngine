#version 450

#define MAX_DIR_LIGHT 1
#define MAX_POINT_LIGHT 2
#define MAX_SPOT_LIGHT 2

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outColorBright;

layout(location = 0)  in VS_OUT {
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
} fs_in;

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

layout(set = 0, binding = 1) uniform sampler2DArray directionalShadowSampler;
layout(set = 0, binding = 2) uniform samplerCubeArray pointShadowSampler;

layout(set = 2, binding = 0) uniform sampler2D tex;
layout(set = 2, binding = 1) uniform sampler2D normalMap;

vec3 fragNormal = fs_in.normal;

float CalcShadow(vec4 fragPos, vec3 lightDirection, uint samplerIdx) {
	vec3 projCoords = fragPos.xyz / fragPos.w;
	projCoords.xy = projCoords.xy * 0.5 + 0.5;

	if (projCoords.z >= 1) return 1;

	vec3 lightDir = normalize(-lightDirection);
	float bias = max(0.0001, 0.001 * (1.0 - dot(fragNormal, lightDir)));
	float shadow = 0.0;
	vec2 texelSize = 1.0 / textureSize(directionalShadowSampler, 0).xy;

	float currentDepth = projCoords.z - bias;

	for(int x = -1; x <= 1; x++) {
		for(int y = -1; y <= 1; y++) {
			vec2 coords = projCoords.xy + vec2(x, y) * texelSize;
			float pcfDepth = texture(directionalShadowSampler, vec3(coords, samplerIdx)).r;
			shadow += currentDepth > pcfDepth ? 1.0 : 0.0;
		}
	}
	shadow /= 9;

	return 1 - shadow;
}

float CalcPointShadow(vec3 fragPos, vec3 lightPos, float farPlane, uint lightIdx) {
	vec3 fragToLight = fragPos - lightPos;
	vec3 lightDir = normalize(-fragToLight);
	float bias = max(0.01, 0.1 * (1.0 - dot(fragNormal, lightDir)));

	if (length(fragToLight) >= farPlane) return 1;

	float shadow = 0.0;
	int samples = 20;
	float viewDistance = length(sceneData.cameraData.pos - fs_in.fragPos);
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
		vec3 coords = fragToLight + sampleOffsetDirections[i] * diskRadius;
		float closestDepth = texture(pointShadowSampler, vec4(coords, lightIdx)).r;
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
		vec3 lightDir = normalize(fs_in.tangentDirLightDirection[i]);
		float lightAngle = clamp(dot(fragNormal, lightDir), 0.0, 1.0);
		vec3 lightColor = light.color.xyz * light.color.w;

		float shadow = 0.f;
		if (lightAngle > 0.01) 
			shadow = CalcShadow(light.viewProj * fs_in.worldCoords, light.direction, i);

		vec3 ambient = light.ambient * lightColor;

		vec3 diffuse = light.diffuse * lightAngle * lightColor;

		vec3 viewDir = normalize(fs_in.tangentViewPos - fs_in.tangentFragPos);
		vec3 halfwayDir = normalize(lightDir + viewDir);
		vec3 specular = light.specular * pow(max(0.0, dot(fragNormal, halfwayDir)), 32) * lightColor;

		result += ambient + shadow * (diffuse + specular);
	}
	return result;
}

vec3 CalcPoint() {
	vec3 result = vec3(0.f);
	for(int i = 0; i < sceneData.pointLightsCount; i++) {
		PointLight light = sceneData.pointLights[i];
		vec3 lightColor = light.color.xyz * light.color.w;
		vec3 lightDir = normalize(fs_in.tangentPointLightPos[i] - fs_in.tangentFragPos);
		float lightAngle = clamp(dot(fragNormal, lightDir), 0.0, 1.0);

		float shadow = 0.f;
		if (lightAngle > 0.01) shadow = CalcPointShadow(fs_in.fragPos, light.position, light.farPlane, i);

		vec3 ambient = light.ambient * lightColor;

		vec3 diffuse = light.diffuse * lightAngle * lightColor;

		vec3 viewDir =normalize(fs_in.tangentViewPos - fs_in.tangentFragPos);
		vec3 halfwayDir = normalize(lightDir + viewDir);
		vec3 specular = light.specular * pow(max(0.0, dot(fragNormal, halfwayDir)), 32) * lightColor;

		float dist = distance(light.position, fs_in.fragPos);
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
		vec3 lightDir = normalize(fs_in.tangentSpotLightPos[i] - fs_in.tangentFragPos);
		float phi = cos(radians(light.cutOffInner));
		float gamma = cos(radians(light.cutOffOuter));
		float theta = dot(lightDir, normalize(-light.direction));
		float intensity = smoothstep(0.0, 1.0, (theta - gamma) / (phi - gamma));

		vec3 ambient = light.ambient * lightColor;

		float lightAngle = clamp(dot(fragNormal, lightDir), 0.0, 1.0);
		vec3 diffuse = light.diffuse * lightAngle * lightColor;

		vec3 viewDir = normalize(fs_in.tangentViewPos - fs_in.tangentFragPos);
		vec3 halfwayDir = normalize(lightDir + viewDir);
		vec3 specular = light.specular * pow(max(0.0, dot(fragNormal, halfwayDir)), 32) * lightColor;

		result += ambient + (diffuse + specular) * intensity;
	}
	return result;
}

void main() {
	vec3 tex_color = texture(tex, fs_in.textureCoords).xyz;

	vec3 normal = texture(normalMap, fs_in.textureCoords).xyz;
	fragNormal = (normal * 2.0 - 1.0);

	vec3 directional = CalcDirectional();
	vec3 point = CalcPoint();
	vec3 spot = CalcSpot();

	vec3 color = tex_color * (directional + point + spot);
	outColor = vec4(color, 1.f);
	
	float brightness = dot(outColor.rgb, vec3(0.2126, 0.7152, 0.0722));
	if (brightness > 1.0)
		outColorBright = outColor;
	else 
		outColorBright = vec4(0.0, 0.0, 0.0, 1.0);
}