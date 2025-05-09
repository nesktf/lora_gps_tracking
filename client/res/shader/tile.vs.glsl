#version 460 core

layout (location = 0) in vec3 att_coords;
layout (location = 1) in vec3 att_normals;
layout (location = 2) in vec2 att_texcoords;
out vec2 tex_coord;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;

void main() {
  gl_Position = u_proj * u_view * u_model * vec4(att_coords, 1.0f);
  tex_coord = att_texcoords;
}

