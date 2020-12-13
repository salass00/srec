#version 310 es

#ifdef GL_ES
precision mediump float;
#endif

in vec2 TexCoord;

uniform layout(location = 0) vec3 RGBMultiplier;
uniform layout(location = 1) float Offset;

uniform sampler2D TexSampler;

out vec4 FragColor;

void main(void) {
	vec4 texel = texture(TexSampler, TexCoord);

	float value = dot(RGBMultiplier, texel.rgb) + Offset;

	FragColor = vec4(0.0, 0.0, 0.0, value);
}
