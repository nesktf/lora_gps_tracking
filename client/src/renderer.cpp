#include "renderer.hpp"

render_ctx::render_ctx(ntf::renderer_window&& win, ntf::renderer_context&& render,
                   ntf::quad_mesh&& quad, ntf::renderer_pipeline&& quad_pipeline,
                   ntf::font_renderer&& frenderer, ntf::sdf_text_rule&& frule,
                   const ntf::mat4& proj, const ntf::mat4& view, vec2 cam_origin) :
  _win{std::move(win)}, _ctx{std::move(render)},
  _quad{std::move(quad)}, _tile_pipeline{std::move(quad_pipeline)},
  _frenderer{std::move(frenderer)}, _frule{std::move(frule)},
  _proj{proj}, _view{view}, _cam_pos{0.f, 0.f}, _cam_origin{cam_origin} {}

render_ctx& render_ctx::construct(std::string_view tile_vert_src, std::string_view tile_frag_src,
                                  ntf::font_atlas_data&& font_atlas, ntf::extent2d win_sz) {
  const ntf::win_gl_params gl_param {
    .ver_major = 4,
    .ver_minor = 3,
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

  const auto attr_desc = ntf::quad_mesh::attr_descriptor();
  ntf::r_blend_opts blending {
    .mode = ntf::r_blend_mode::add,
    .src_factor = ntf::r_blend_factor::src_alpha,
    .dst_factor = ntf::r_blend_factor::inv_src_alpha,
    .color = {0.f, 0.f, 0.f, 0.f},
    .dynamic = false,
  };
  ntf::r_shader stages[] {vert.handle(), frag.handle()};
  auto pip = ntf::renderer_pipeline::create(*rctx, {
    .attrib_binding = 0u,
    .attrib_stride = sizeof(ntf::quad_mesh::attr_type),
    .attribs = attr_desc,
    .stages = stages,
    .primitive = ntf::r_primitive::triangles,
    .poly_mode = ntf::r_polygon_mode::line,
    .poly_width = ntf::nullopt,
    .stencil_test = nullptr,
    .depth_test = nullptr,
    .scissor_test = nullptr,
    .face_culling = nullptr,
    .blending = blending,
  }).value();

  auto frenderer = ntf::font_renderer::create(*rctx, std::move(font_atlas)).value();

  vec2 cam_origin {win_sz.x*.5f, win_sz.y*.5f};
  ntf::mat4 proj_mat = glm::ortho(0.f, (float)win_sz.x, 0.f, (float)win_sz.y);
  ntf::mat4 view_mat = ntf::build_view_matrix(vec2{0.f, 0.f}, cam_origin,
                                              vec2{1.f, 1.f}, ntf::vec3{0.f, 0.f, 0.f});
  auto sdf_rule = ntf::sdf_text_rule::create(*rctx, proj_mat,
                                             ntf::color3{.9f, .9f, .9f}, 0.5f, 0.05f,
                                             ntf::color3{0.f, 0.f, 0.f},
                                             // ntf::vec2{-0.005f, -0.005f},
                                             ntf::vec2{0.f, 0.f},
                                             0.7f, 0.08f).value();

  return _construct(std::move(*win), std::move(*rctx),
                    std::move(quad), std::move(pip),
                    std::move(frenderer), std::move(sdf_rule),
                    proj_mat, view_mat, cam_origin);
}

void render_ctx::_on_render([[maybe_unused]] float dt) {
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

void render_ctx::render_texture(size_t tex, const ntf::mat4& transf) {
  auto fbo = ntf::renderer_framebuffer::default_fbo(_ctx);
  const int sampler = 0;
  const ntf::r_push_constant unifs[] = {
    ntf::r_format_pushconst(*_tile_pipeline.uniform("u_model"), transf),
    ntf::r_format_pushconst(*_tile_pipeline.uniform("u_proj"), _proj),
    ntf::r_format_pushconst(*_tile_pipeline.uniform("u_view"), _view),
    ntf::r_format_pushconst(*_tile_pipeline.uniform("u_sampler"), sampler),
  };
  const ntf::r_buffer_binding bbinds[] = {
    {.buffer = _quad.vbo().handle(), .type = ntf::r_buffer_type::vertex, .location = {}},
    {.buffer = _quad.ebo().handle(), .type = ntf::r_buffer_type::index, .location = {}},
  };
  const ntf::r_texture_binding tbinds[] = {
    {.texture = _texs[tex].handle(), .location = sampler},
  };
  _ctx.submit_command({
    .target = fbo.handle(),
    .pipeline = _tile_pipeline.handle(),
    .buffers = bbinds,
    .textures = tbinds,
    .uniforms = unifs,
    .draw_opts = {
      .count = 6,
      .offset = 0,
      .instances = 0,
      .sort_group = 0
    },
    .on_render = {},
  });
}

void render_ctx::update_viewport(ntf::uint32 w, ntf::uint32 h) {
  ntf::renderer_framebuffer::default_fbo(_ctx).viewport({0, 0, w, h});
  _proj = glm::ortho(0.f, (float)w, 0.f, (float)h);
  _cam_origin.x = w*.5f;
  _cam_origin.y = h*.5f;
  _gen_view();
  _frule.transform(_proj);
}

void render_ctx::_gen_view() {
  _view = ntf::build_view_matrix(_cam_pos, _cam_origin,
                                 vec2{1.f, 1.f}, ntf::vec3{0.f, 0.f, 0.f});
}
