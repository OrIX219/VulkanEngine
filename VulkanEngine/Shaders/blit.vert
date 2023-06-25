#version 460

layout(location = 0) out vec2 textureCoords;

void main() {
	textureCoords = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(textureCoords * 2.0f - 1.0f, 0.1f, 1.0f);
}