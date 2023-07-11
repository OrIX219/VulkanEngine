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
	mat4 viewProj;
	vec3 pos;
};

layout(set = 0, binding = 0) uniform SceneData {
	CameraData cameraData;
} sceneData;

void generateLine(int index) {
	gl_Position = sceneData.cameraData.viewProj * gl_in[index].gl_Position;
	EmitVertex();
	gl_Position = sceneData.cameraData.viewProj *
		(gl_in[index].gl_Position + vec4(gs_in[index].normal, 0) * magnitude);
	EmitVertex();
	EndPrimitive();
}

void main() {
	generateLine(0);
	generateLine(1);
	generateLine(2);
}