#version 330 core

in vec2 tex_coord;
out vec4 frag_color;

uniform sampler2D tex;
uniform vec4 text_color;

void main() {
  vec4 sampled = vec4(1.0f, 1.0f, 1.0f, texture(tex, tex_coord).r);
  frag_color = text_color * sampled;
}

