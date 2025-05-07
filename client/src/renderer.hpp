#pragma once

#include <shogle/render.hpp>
#include <shogle/assets.hpp>
#include <shogle/scene.hpp>
#include <shogle/math.hpp>
#include <shogle/version.hpp>
#include <shogle/stl.hpp>

using logger = ntf::logger;
using ntf::uint32;
using ntf::int32;
using ntf::ivec2;
using ntf::dvec2;
using ntf::vec2;

class render_ctx : public ntf::singleton<render_ctx> {
private:
  render_ctx(ntf::renderer_window&& win, ntf::renderer_context&& render,
             ntf::quad_mesh&& quad, ntf::renderer_pipeline&& tile_pipeline,
             ntf::font_renderer&& frenderer, ntf::sdf_text_rule&& frule,
             const ntf::mat4& proj, const ntf::mat4& view, vec2 cam_origin);

public:
  static render_ctx& construct(std::string_view tile_vert_src, std::string_view tile_frag_src,
                               ntf::font_atlas_data&& font_atlas, ntf::extent2d win_sz);

public:
  size_t make_texture(const ntf::image_data& image);
  void render_texture(size_t tex, const ntf::mat4& transf);
  void update_viewport(ntf::uint32 w, ntf::uint32 h);

  void cam_pos(float x, float y) {
    _cam_pos.x = x;
    _cam_pos.y = y;
    _gen_view();
  }
  vec2 cam_pos() const { return _cam_pos; }

public:
  ntf::renderer_window& window() { return _win; }
  ntf::renderer_context& ctx() { return _ctx; }

private:
  void _on_render(float dt);
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
  void start_loop(F&& fun) {
    ntf::shogle_render_loop(_win, _ctx, [&](float dt){
      _text_buff.clear();
      fun(dt);
      _on_render(dt);
    });
  }

private:
  ntf::renderer_window _win;
  ntf::renderer_context _ctx;
  ntf::quad_mesh _quad;
  ntf::renderer_pipeline _tile_pipeline;
  ntf::font_renderer _frenderer;
  ntf::sdf_text_rule _frule;
  ntf::mat4 _proj, _view;
  ntf::text_buffer _text_buff;
  std::vector<ntf::renderer_texture> _texs;
  vec2 _cam_pos;
  vec2 _cam_origin;

private:
  friend ntf::singleton<render_ctx>;
};
