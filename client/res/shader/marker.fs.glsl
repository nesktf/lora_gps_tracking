#version 460 core

out vec4 frag_color;

uniform float u_point_rad;
uniform float u_pres_rad;
uniform vec2 u_pos;
uniform mat4 u_view;

const vec4 point_color = vec4(.164f, .715f, .965f, 1.f);
const vec4 point_outline_color = vec4(.1f, .1f, .1f, 1.f);
const float point_outline_width = 1.5f;

const vec4 pres_color = vec4(.703f, .898f, .988f, .3f);
const vec4 pres_outline_color = vec4(.4f, .4f, .4f, .5f);
const float pres_outline_width = 1.f;

float circle_dist(vec2 p, float radius) {
	return length(p) - radius;
}

float sdf_mask(float dist) {
  return clamp(-dist, 0.f, 1.f);
}

float sdf_outline_mask(float dist, float width) {
  float alpha1 = clamp(dist + width, 0.f, 1.f);
  float alpha2 = clamp(dist, 0.f, 1.f);
  return alpha1 - alpha2;
}

vec4 fill_sdf(float dist, vec4 color) {
  return mix(vec4(0.f), color, clamp(-dist, 0.f, 1.f));
}

void main() {
	vec4 pos = u_view*vec4(-gl_FragCoord.xy+u_pos, 0.f, 1.f);

  vec4 out_color = vec4(0.f);

  float pres_dist = circle_dist(pos.xy, u_pres_rad);
  out_color = mix(out_color, pres_color, sdf_mask(pres_dist));
  out_color = mix(out_color, pres_outline_color, sdf_outline_mask(pres_dist, pres_outline_width));

  float point_dist = circle_dist(pos.xy, u_point_rad);
  out_color = mix(out_color, point_color, sdf_mask(point_dist));
  out_color = mix(out_color, point_outline_color, sdf_outline_mask(point_dist, point_outline_width));

  frag_color = out_color;
}
