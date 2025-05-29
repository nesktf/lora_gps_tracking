#pragma once

#include "./renderer.hpp"

class gps_marker final : public rendering_rule {
private:
  struct shader_data {
    ntf::mat4 view;
    ntf::vec2 pos;
    float point_rad;
    float pres_rad;
  };

private:
  gps_marker(pipeline_t pipeline, buffer_t uniform_buffer,
             float point_rad, float pres_rad) noexcept;

public:
  static gps_marker make_marker(float size, float radius);

public:
  std::pair<pipeline_t, buffer_t> write_uniforms() override;

  void set_pos(vec2 pos) { _pos = pos; }
  void set_radius(float radius) { _pres_rad = radius; }
  void set_size(float size) { _point_rad = size; }

public:
  vec2 pos() const { return _pos; }
  float size() const { return _point_rad; }

private:
  pipeline_t _pipeline;
  buffer_t _uniform_buffer;
  float _point_rad, _pres_rad;
  vec2 _pos;
};

class map_shape final : public rendering_rule {
private:
  struct shader_data {
    ntf::mat4 view;
    color4 color;
    color4 out_color;
    vec2 pos;
    float radius;
    float rot;
    float out_width;
    float nsides;
  };

public:
  enum shape_enum {
    S_CIRCLE,
    S_TRIANGLE,
    S_SQUARE,
    S_DIAMOND,
    S_PENTAGON,
  };

private:
  map_shape(pipeline_t pipeline, buffer_t uniform_buffer, const color4& color,
            float nsides, float radius, float rot) noexcept;

public:
  static map_shape make_shape(shape_enum shape, float size, const color4& color);

public:
  std::pair<pipeline_t, buffer_t> write_uniforms() override;

  void set_pos(vec2 pos) { _pos = pos; }
  void set_size(float size) { _radius = size; }
  void set_color(const color4& col) { _color = col; }
  void set_outline_color(const color4& col) { _color_out = col; }
  void set_outline_width(float width) { _out_width = width; }
  void set_rot(float rot) { _rot = rot; }

public:
  vec2 pos() const { return _pos; }
  float size() const { return _radius; }

private:
  pipeline_t _pipeline;
  buffer_t _uniform_buffer;
  color4 _color, _color_out;
  float _nsides, _radius, _rot, _out_width;
  vec2 _pos;
};

class bezier_thing : public rendering_rule {
public:
  bezier_thing(pipeline_t pipeline);

public:
  std::pair<pipeline_t, buffer_t> write_uniforms() override;

public:
  pipeline_t _pipeline;
};
