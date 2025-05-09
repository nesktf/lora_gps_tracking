#version 460 core

layout (location = 0) in vec3 att_coords;
layout (location = 1) in vec3 att_normals;
layout (location = 2) in vec2 att_texcoords;

void main() {
  gl_Position = vec4(att_coords.x*2.f, att_coords.y*2.f, att_coords.z*2.f, 1.f);
}

