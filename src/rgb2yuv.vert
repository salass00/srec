#version 300 es

#ifdef GL_ES
precision mediump float;
#endif

in layout(location = 0) vec2 vertPosition;
in layout(location = 1) vec2 vertTexCoord;

out vec2 TexCoord;

void main(void) {
	gl_Position = vec4(vertPosition, 0.0, 1.0);

	TexCoord = vertTexCoord;
}
