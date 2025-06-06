#include "renderer.hpp"

std::string_view vert_frag_only_src = R"glsl(
#version 460 core

layout (location = 0) in vec3 att_coords;
layout (location = 1) in vec3 att_normals;
layout (location = 2) in vec2 att_texcoords;

void main() {
  gl_Position = vec4(att_coords.x*2.f, att_coords.y*2.f, att_coords.z*2.f, 1.f);
}
)glsl";

render_ctx::render_ctx(ntf::renderer_window&& win, ntf::renderer_context&& render,
                       ntf::quad_mesh&& quad, ntf::renderer_pipeline&& quad_pipeline,
                       ntf::font_renderer&& frenderer, ntf::sdf_text_rule&& frule,
                       const ntf::mat4& proj, ntf::extent2d viewport) :
  _win{std::move(win)}, _ctx{std::move(render)},
  _quad{std::move(quad)}, _tile_pipeline{std::move(quad_pipeline)},
  _frenderer{std::move(frenderer)}, _frule{std::move(frule)},
  _vp{viewport}, _proj{proj}, _inv_proj{glm::inverse(proj)},
  _cam_pos{0.f, 0.f}, _cam_origin{(float)viewport.x / 2.f, (float)viewport.y / 2.f}
{
  _gen_view();
}

render_ctx& render_ctx::construct(std::string_view tile_vert_src, std::string_view tile_frag_src,
                                  ntf::font_atlas_data&& font_atlas, ntf::extent2d win_sz) {
  const ntf::win_gl_params gl_param {
    .ver_major = 4,
    .ver_minor = 6,
  };
  auto win = ntf::renderer_window::create({
    .width = win_sz.x,
    .height = win_sz.y,
    .title = "test - osm_client",
    .x11_class_name = "osm_client",
    .x11_instance_name = nullptr,
    .ctx_params = gl_param,
  });
  if (!win) {
    logger::error("Failed to init window");
    std::exit(1);
  }
  auto rctx = ntf::renderer_context::create({
    .window = win->handle(),
    .renderer_api = win->renderer(),
    .swap_interval = 1,
    .fb_viewport = {0, 0, win_sz.x, win_sz.y},
    .fb_clear = ntf::r_clear_flag::color_depth,
    .fb_color = {.3f, .3f, .3f, 1.f},
    .alloc = nullptr,
  });
  if (!rctx) {
    logger::error("Failed to init render context");
    std::exit(1);
  }

  auto quad = ntf::quad_mesh::create(*rctx).value();

  auto vert = ntf::renderer_shader::create(*rctx, {
    .type = ntf::r_shader_type::vertex,
    .source = {tile_vert_src},
  }).value();
  auto frag = ntf::renderer_shader::create(*rctx, {
    .type = ntf::r_shader_type::fragment,
    .source = {tile_frag_src},
  }).value();

  const auto attributes = ntf::quad_mesh::attribute_binding();
  const ntf::r_blend_opts blending {
    .mode = ntf::r_blend_mode::add,
    .src_factor = ntf::r_blend_factor::src_alpha,
    .dst_factor = ntf::r_blend_factor::inv_src_alpha,
    .color = {0.f, 0.f, 0.f, 0.f},
  };
  ntf::r_shader stages[] {vert.handle(), frag.handle()};
  auto pip = ntf::renderer_pipeline::create(*rctx, {
    .attributes = attributes,
    .stages = stages,
    .primitive = ntf::r_primitive::triangles,
    .poly_mode = ntf::r_polygon_mode::fill,
    .poly_width = ntf::nullopt,
    .stencil_test = nullptr,
    .depth_test = nullptr,
    .scissor_test = nullptr,
    .face_culling = nullptr,
    .blending = blending,
  }).value();

  ntf::mat4 proj_mat = glm::ortho(0.f, (float)win_sz.x, 0.f, (float)win_sz.y);
  auto frenderer = ntf::font_renderer::create(*rctx, proj_mat, std::move(font_atlas)).value();

  vec2 cam_origin {win_sz.x*.5f, win_sz.y*.5f};
  auto sdf_rule = ntf::sdf_text_rule::create(*rctx,
                                             ntf::color3{.9f, .9f, .9f}, 0.5f, 0.05f,
                                             ntf::color3{0.f, 0.f, 0.f},
                                             // ntf::vec2{-0.005f, -0.005f},
                                             ntf::vec2{0.f, 0.f},
                                             0.7f, 0.08f).value();

  return _construct(std::move(*win), std::move(*rctx),
                    std::move(quad), std::move(pip),
                    std::move(frenderer), std::move(sdf_rule),
                    proj_mat, win_sz);
}

void render_ctx::start_render() {
  _text_buff.clear();
}

void render_ctx::end_render() {
  auto fbo = ntf::renderer_framebuffer::default_fbo(_ctx);
  _frenderer.clear_state();
  _frenderer.append_text(_text_buff);
  _frenderer.render(_quad, fbo, _frule);
}

size_t render_ctx::make_texture(const ntf::image_data& image) {
  const auto desc = image.make_descriptor();
  _texs.emplace_back(ntf::renderer_texture::create(_ctx, {
    .type = ntf::r_texture_type::texture2d,
    .format = image.format,
    .extent = image.extent,
    .layers = 1,
    .levels = 1,
    .images = {desc},
    .gen_mipmaps = false,
    .sampler = ntf::r_texture_sampler::nearest,
    .addressing = ntf::r_texture_address::clamp_edge,
  }).value());
  return _texs.size()-1;
}

void render_ctx::render_texture(size_t tex, const ntf::mat4& transf, uint32 sort) {
  auto fbo = ntf::renderer_framebuffer::default_fbo(_ctx);
  const ntf::r_push_constant unifs[] = {
    ntf::r_format_pushconst(*_tile_pipeline.uniform("u_model"), transf),
    ntf::r_format_pushconst(*_tile_pipeline.uniform("u_proj"), _proj),
    ntf::r_format_pushconst(*_tile_pipeline.uniform("u_view"), _view),
    ntf::r_format_pushconst(*_tile_pipeline.uniform("u_sampler"), 0),
  };
  auto tex_binding = _texs[tex].handle();
  _ctx.submit_command({
    .target = fbo.handle(),
    .pipeline = _tile_pipeline.handle(),
    .buffers = _quad.bindings(),
    .textures = {tex_binding},
    .uniforms = unifs,
    .draw_opts = {
      .count = 6,
      .offset = 0,
      .instances = 0,
    },
    .sort_group = sort,
    .on_render = {},
  });
}

void render_ctx::update_viewport(ntf::uint32 w, ntf::uint32 h) {
  ntf::renderer_framebuffer::default_fbo(_ctx).viewport({0, 0, w, h});
  _vp.x = w;
  _vp.y = h;
  _proj = glm::ortho(0.f, (float)w, 0.f, (float)h);
  _inv_proj = glm::inverse(_proj);
  _cam_origin.x = w*.5f;
  _cam_origin.y = h*.5f;
  _gen_view();
  _frenderer.set_transform(_proj);
}

void render_ctx::_gen_view() {
  _view = ntf::build_view_matrix(_cam_pos, _cam_origin, vec2{1.f, 1.f}, ntf::vec3{0.f, 0.f, 0.f});
}

vec2 render_ctx::raycast(float x, float y) const {
  const auto pos = _inv_proj*ntf::vec4{
    (2.f*x) / (float)_vp.x - 1.f,
    (1.f - (2.f*y)) / (float)_vp.y,
    -1.f, 0.f
  };
  return {pos.x + _cam_pos.x, pos.y + _cam_pos.y + _vp.y*.5f};
}

pipeline_t render_ctx::make_pipeline(std::string_view vert_src, std::string_view frag_src) {
  auto vert = ntf::renderer_shader::create(_ctx, {
    .type = ntf::r_shader_type::vertex,
    .source = {vert_src},
  }).value();
  auto frag = ntf::renderer_shader::create(_ctx, {
    .type = ntf::r_shader_type::fragment,
    .source = {frag_src},
  }).value();

  const ntf::r_blend_opts blending {
    .mode = ntf::r_blend_mode::add,
    .src_factor = ntf::r_blend_factor::src_alpha,
    .dst_factor = ntf::r_blend_factor::inv_src_alpha,
    .color = {0.f, 0.f, 0.f, 0.f},
  };

  const auto attributes = ntf::quad_mesh::attribute_binding();
  const ntf::r_shader stages[] {vert.handle(), frag.handle()};
  _pips.emplace_back(ntf::renderer_pipeline::create(_ctx, {
    .attributes = attributes,
    .stages = stages,
    .primitive = ntf::r_primitive::triangles,
    .poly_mode = ntf::r_polygon_mode::fill,
    .poly_width = ntf::nullopt,
    .stencil_test = nullptr,
    .depth_test = nullptr,
    .scissor_test = nullptr,
    .face_culling = nullptr,
    .blending = blending,
  }).value());

  return _pips.size()-1u;
}

buffer_t render_ctx::make_buffer(size_t size) {
  _buffs.emplace_back(ntf::renderer_buffer::create(_ctx, {
    .type = ntf::r_buffer_type::uniform,
    .flags = ntf::r_buffer_flag::dynamic_storage,
    .size = size,
    .data = nullptr,
  }).value());

  return _buffs.size()-1u;
}

void render_ctx::render_thing(rendering_rule& rule, uint32 sort) {
  auto fbo = ntf::renderer_framebuffer::default_fbo(_ctx);
  auto [pip, buff] = rule.write_uniforms();
  NTF_ASSERT(buff < _buffs.size());
  NTF_ASSERT(pip < _pips.size());
  const ntf::r_shader_buffer unif_buff {
    .buffer = _buffs[buff].handle(),
    .binding = 1u,
    .offset = 0u,
    .size = _buffs[buff].size(),
  };
  _ctx.submit_command({
    .target = fbo.handle(),
    .pipeline = _pips[pip].handle(),
    .buffers = _quad.bindings({unif_buff}),
    .textures = {},
    .uniforms = {},
    .draw_opts = {
      .count = 6,
      .offset = 0,
      .instances = 0,
    },
    .sort_group = sort,
    .on_render = {},
  });
}
