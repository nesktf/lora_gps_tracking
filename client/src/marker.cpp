#include "./marker.hpp"

gps_marker::gps_marker(pipeline_t handle, buffer_t uniform_buffer,
                       float point_rad, float pres_rad) noexcept :
  _pipeline{handle}, _uniform_buffer{uniform_buffer},
  _point_rad{point_rad}, _pres_rad{pres_rad},
  _pos{0.f, 0.f} {}

std::pair<pipeline_t, buffer_t> gps_marker::write_uniforms() {
  auto& r = render_ctx::instance();
  const auto buff = r.get_buffer(_uniform_buffer); 
  const shader_data data {
    .view = r.get_view(),
    .pos = _pos,
    .point_rad = _point_rad,
    .pres_rad = _pres_rad,
  };
  buff.upload(0u, sizeof(shader_data), &data);
  return std::make_pair(_pipeline, _uniform_buffer);
}

map_shape::map_shape(pipeline_t pipeline, buffer_t uniform_buffer, const color4& color,
                     float nsides, float radius, float rot) noexcept :
  _pipeline{pipeline}, _uniform_buffer{uniform_buffer},
  _color{color}, _color_out{0.f, 0.f, 0.f, 1.f},
  _nsides{nsides}, _radius{radius}, _rot{rot}, _out_width{0.f},
  _pos{0.f, 0.f} {}

std::pair<pipeline_t, buffer_t> map_shape::write_uniforms() {
  auto& r = render_ctx::instance();
  const auto buff = r.get_buffer(_uniform_buffer); 
  const shader_data data {
    .view = r.get_view(),
    .color = _color,
    .out_color = _color_out,
    .pos = _pos,
    .radius = _radius,
    .rot = _rot,
    .out_width = _out_width,
    .nsides = _nsides,
  };
  buff.upload(0u, sizeof(shader_data), &data);
  return std::make_pair(_pipeline, _uniform_buffer);
}

static constexpr std::string_view frag_gps_marker = R"glsl(
#version 460 core

out vec4 frag_color;

layout (std140, binding = 1) uniform frag_data {
  mat4 u_view;
  vec2 u_pos;
  float u_point_rad;
  float u_pres_rad;
};

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

layout (std140, binding = 1) uniform frag_data {
  mat4 u_view;
  vec4 u_color;
  vec4 u_out_color;
  vec2 u_pos;
  float u_radius;
  float u_rot;
  float u_out_width;
  float u_nsides;
};

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
  auto& r = render_ctx::instance();
  auto pip = r.make_pipeline(vert_frag_only_src, frag_gps_marker);
  auto buff = r.make_buffer(sizeof(shader_data));
  return gps_marker{pip, buff, size, radius};
}

map_shape map_shape::make_shape(shape_enum shape, float size, const color4& color)
{
  auto& r = render_ctx::instance();
  auto pip = r.make_pipeline(vert_frag_only_src, frag_shape);
  auto buff = r.make_buffer(sizeof(shader_data));
  switch (shape) {
    case shape_enum::S_CIRCLE: {
      return map_shape{pip, buff, color, 0.f, size, 0.f};
      break;
    }
    case shape_enum::S_TRIANGLE: {
      return map_shape{pip, buff, color, 3.f, size, M_PIf};
      break;
    }
    case shape_enum::S_SQUARE: {
      return map_shape{pip, buff, color, 4.f, size, 0.f};
      break;
    }
    case shape_enum::S_DIAMOND: {
      return map_shape{pip, buff, color, 4.f, size, M_PIf*.25f};
      break;
    }
    case shape_enum::S_PENTAGON: {
      return map_shape{pip, buff, color, 5.f, size, M_PIf};
      break;
    }
  }
  NTF_UNREACHABLE();
}
