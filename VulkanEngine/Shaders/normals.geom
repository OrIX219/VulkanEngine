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

layout(set = 0, binding = 0) uniform SceneData {
	CameraData cameraData;
	vec4 ambientColor;
	vec4 fogColor;
	vec4 fogDistances;
	vec4 sunlightDirection;
	vec4 sunlightColor;
	mat4 sunlightShadowMat;
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