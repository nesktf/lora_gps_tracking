#pragma once

#include <shogle/render.hpp>
#include <shogle/assets.hpp>
#include <shogle/scene.hpp>
#include <shogle/math.hpp>
#include <shogle/version.hpp>
#include <shogle/stl.hpp>
#include <shogle/boilerplate.hpp>

using logger = ntf::logger;
using ntf::uint32;
using ntf::int32;
using ntf::ivec2;
using ntf::dvec2;
using ntf::vec2;
using ntf::color4;

using pipeline_t = size_t;
using buffer_t = size_t;

extern std::string_view vert_frag_only_src;

struct rendering_rule {
  virtual ~rendering_rule() = default;
  virtual std::pair<pipeline_t, buffer_t> write_uniforms() = 0;
};

class render_ctx : public ntf::singleton<render_ctx> {
private:
  render_ctx(ntf::renderer_window&& win, ntf::renderer_context&& render,
             ntf::quad_mesh&& quad, ntf::renderer_pipeline&& tile_pipeline,
             ntf::font_renderer&& frenderer, ntf::sdf_text_rule&& frule,
             const ntf::mat4& proj, ntf::extent2d viewport);

public:
  static render_ctx& construct(std::string_view tile_vert_src, std::string_view tile_frag_src,
                               ntf::font_atlas_data&& font_atlas, ntf::extent2d win_sz);

public:
  size_t make_texture(const ntf::image_data& image);
  pipeline_t make_pipeline(std::string_view vert, std::string_view frag);
  buffer_t make_buffer(size_t size);
  void render_texture(size_t tex, const ntf::mat4& transf, uint32 sort = 0u);
  void update_viewport(ntf::uint32 w, ntf::uint32 h);
  vec2 viewport() const { return _vp; }

  void cam_pos(float x, float y) {
    _cam_pos.x = x;
    _cam_pos.y = y;
    _gen_view();
  }
  vec2 cam_pos() const { return _cam_pos; }

  vec2 raycast(float x, float y) const;

public:
  void render_thing(rendering_rule& rule, uint32 sort = 0u);

  void start_render();
  void end_render();

public:
  ntf::renderer_window& window() { return _win; }
  ntf::renderer_context& ctx() { return _ctx; }

private:
  void _gen_view();

public:
  template<typename... Args>
  void render_text(float x, float y, float scale, fmt::format_string<Args...> fmt, Args&&... arg) {
    _text_buff.append_fmt(_frenderer.glyphs(), x, y, scale, fmt, std::forward<Args>(arg)...);
  }

  void render_string(float x, float y, float scale, std::string_view str) {
    _text_buff.append(_frenderer.glyphs(), x, y, scale, str);
  }

public:
  template<typename F>
  void start_loop(const uint32& ups, F&& fun) {
    ntf::shogle_render_loop(_win, _ctx, ups, std::forward<F>(fun));
  }

public:
  const ntf::mat4& get_proj() const { return _proj; }
  const ntf::mat4& get_view() const { return _view; }
  ntf::r_pipeline_view get_pipeline(pipeline_t idx) const { return _pips[idx].handle(); } 
  ntf::r_buffer_view get_buffer(buffer_t idx) const { return _buffs[idx].handle(); }

private:
  ntf::renderer_window _win;
  ntf::renderer_context _ctx;
  ntf::quad_mesh _quad;
  ntf::renderer_pipeline _tile_pipeline;
  ntf::font_renderer _frenderer;
  ntf::sdf_text_rule _frule;

  ntf::extent2d _vp;
  ntf::mat4 _proj, _inv_proj, _view;
  vec2 _cam_pos;
  vec2 _cam_origin;

  ntf::text_buffer _text_buff;
  std::vector<ntf::renderer_texture> _texs;
  std::vector<ntf::renderer_pipeline> _pips;
  std::vector<ntf::renderer_buffer> _buffs;

private:
  friend ntf::singleton<render_ctx>;
};
