#version 330 core

layout(location = 0) in vec4 att_vertex;
out vec2 tex_coord;

uniform mat4 proj;
uniform mat4 model;

void main() {
  gl_Position = proj * model * vec4(att_vertex.xy, 0.0f, 1.0f);
  tex_coord = att_vertex.zw;
}

