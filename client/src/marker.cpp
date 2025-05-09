#include "./marker.hpp"

#define SET_UNIFORM(name, var) \
  list.emplace_back(ntf::r_format_pushconst(*pip.uniform(name), var))

gps_marker::gps_marker(pipeline_t handle, float point_rad, float pres_rad) noexcept :
  _pipeline{handle},
  _point_rad{point_rad}, _pres_rad{pres_rad},
  _pos{0.f, 0.f} {}

ntf::r_pipeline gps_marker::retrieve_uniforms(ntf::uniform_list& list) {
  const auto pip = render_ctx::instance().get_pipeline(_pipeline);
  const auto& view = render_ctx::instance().get_view();

  SET_UNIFORM("u_point_rad", _point_rad);
  SET_UNIFORM("u_pres_rad", _pres_rad);
  SET_UNIFORM("u_pos", _pos);
  SET_UNIFORM("u_view", view);

  return pip.handle();
}

map_shape::map_shape(pipeline_t pipeline, const color4& color,
                     float nsides, float radius, float rot) noexcept :
  _pipeline{pipeline},
  _color{color}, _color_out{0.f, 0.f, 0.f, 1.f},
  _nsides{nsides}, _radius{radius}, _rot{rot}, _out_width{0.f},
  _pos{0.f, 0.f} {}

ntf::r_pipeline map_shape::retrieve_uniforms(ntf::uniform_list& list) {
  const auto pip = render_ctx::instance().get_pipeline(_pipeline);
  const auto& view = render_ctx::instance().get_view();

  SET_UNIFORM("u_nsides", _nsides);
  SET_UNIFORM("u_radius", _radius);
  SET_UNIFORM("u_rot", _rot);
  SET_UNIFORM("u_color", _color);
  SET_UNIFORM("u_out_width", _out_width);
  SET_UNIFORM("u_out_color", _color_out);
  SET_UNIFORM("u_pos", _pos);
  SET_UNIFORM("u_view", view);

  return pip.handle();
}


static constexpr std::string_view vert_src = R"glsl(
#version 460 core

layout (location = 0) in vec3 att_coords;
layout (location = 1) in vec3 att_normals;
layout (location = 2) in vec2 att_texcoords;

void main() {
  gl_Position = vec4(att_coords.x*2.f, att_coords.y*2.f, att_coords.z*2.f, 1.f);
}
)glsl";

static constexpr std::string_view frag_gps_marker = R"glsl(
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
  out_color = mix(out_color, point_color,
                  sdf_mask(point_dist));
  out_color = mix(out_color, point_outline_color,
                  sdf_outline_mask(point_dist, point_outline_width));

  frag_color = out_color;
}
)glsl";

static constexpr std::string_view frag_shape = R"glsl(
#version 460 core

#define PI  3.14159
#define PI2 (.5 * PI)
#define TAU (2. * PI)

out vec4 frag_color;

uniform float u_nsides;
uniform float u_radius;
uniform float u_rot;
uniform vec4 u_color;

uniform float u_out_width;
uniform vec4 u_out_color;

uniform vec2 u_pos;
uniform mat4 u_view;

float circle_dist(vec2 p, float radius) {
	return length(p) - radius;
}

mat2 rot_mat(float angle) {
  return mat2(cos(angle), sin(angle), -sin(angle), cos(angle));
}

float nshape_dist(vec2 p, float radius, float nsides, float rot) {
  p *= rot_mat(rot);
  float angle = atan(p.y, p.x) + PI2;
  float split = TAU / nsides;
  return length(p) * cos(split * floor(.5 + angle / split) - angle) - radius;
}

float sdf_mask(float dist) {
  return clamp(-dist, 0.f, 1.f);
}

float sdf_outline_mask(float dist, float width) {
  float alpha1 = clamp(dist + width, 0.f, 1.f);
  float alpha2 = clamp(dist, 0.f, 1.f);
  return alpha1 - alpha2;
}

void main() {
  vec4 pos = u_view*vec4(-gl_FragCoord.xy+u_pos, 0.f, 1.f);

  vec4 out_color = vec4(0.f);

  float dist;
  if (u_nsides <= 1.f) {
    dist = circle_dist(pos.xy, u_radius);
  } else {
    dist = nshape_dist(pos.xy, u_radius, u_nsides, u_rot);
  }
  out_color = mix(out_color, u_color, sdf_mask(dist));
  out_color = mix(out_color, u_out_color, sdf_outline_mask(dist, u_out_width));

  frag_color = out_color;
}
)glsl";

gps_marker gps_marker::make_marker(float size, float radius) {
  auto pip = render_ctx::instance().make_pipeline(vert_src, frag_gps_marker);
  return gps_marker{pip, size, radius};
}

map_shape map_shape::make_shape(shape_enum shape, float size, const color4& color)
{
  auto pip = render_ctx::instance().make_pipeline(vert_src, frag_shape);
  switch (shape) {
    case shape_enum::S_CIRCLE: {
      return map_shape{pip, color, 0.f, size, 0.f};
      break;
    }
    case shape_enum::S_TRIANGLE: {
      return map_shape{pip, color, 3.f, size, M_PIf};
      break;
    }
    case shape_enum::S_SQUARE: {
      return map_shape{pip, color, 4.f, size, 0.f};
      break;
    }
    case shape_enum::S_DIAMOND: {
      return map_shape{pip, color, 4.f, size, M_PIf*.25f};
      break;
    }
    case shape_enum::S_PENTAGON: {
      return map_shape{pip, color, 5.f, size, M_PIf};
      break;
    }
  }
  NTF_UNREACHABLE();
}
