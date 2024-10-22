#include <shogle/render/gl.hpp>
#include <shogle/render/glfw.hpp>

#include <shogle/engine.hpp>
#include <shogle/render/gl/shader.hpp>

#include <shogle/scene/transform.hpp>
#include <shogle/scene/camera.hpp>

#include "osm.hpp"
#include "shogle/assets/common.hpp"
#include <shogle/assets/texture.hpp>

int main() {
  ntf::log::set_level(ntf::log::level::verbose);

  auto glfw = ntf::glfw::init();
  ntf::glfw::set_swap_interval(0);

  ntf::glfw::window<ntf::gl_renderer> window{1024, 1024, "test"};
  auto imgui = ntf::imgui::init(window, ntf::imgui::glfw_gl3_impl{});


  using keycode = ntf::glfw_keys::keycode;
  using keystate = ntf::glfw_keys::keystate;
  using gl = ntf::gl_renderer;

  gl::set_blending(true);


  osm::manager manager{"tile_cache/",
    osm::coord{-24.87034, -65.46616}, // top left
    osm::coord{-24.87910, -65.45532}, // bottom right
    17
  };
  auto bounds = manager.prepare_tiles();

  auto min_tile = osm::coord2tile(osm::coord{-24.87034, -65.46616}, 17);

  auto tsz = manager.tex_size();
  auto coso = tsz.second - tsz.first;

  ntf::log::debug("{} {}", coso.x, coso.y);


  // gl::framebuffer fbo{(ntf::vec2)window.size()*.5f};

  // manager.render_tiles(fbo);

  gl::shader_program::loader loader;

  auto vert = ntf::file_contents("res/shader/framebuffer.vs.glsl");
  auto frag = ntf::file_contents("res/shader/framebuffer.fs.glsl");
  auto shader = loader(vert, frag);

  auto shader2 = loader(ntf::file_contents("res/shader/tile.vs.glsl"), ntf::file_contents("res/shader/tile.fs.glsl"));

  auto transf = ntf::transform2d{}.pos((ntf::vec2)window.size()*.5f).scale(coso.x);
  auto camera = ntf::camera2d{}.pos((ntf::vec2)window.size()*.5f)
    .viewport(window.size()).zfar(1.f).znear(-10.f);


  window.set_viewport_event([&](std::size_t w, std::size_t h) {
    gl::set_viewport(w, h);
    camera.viewport(w, h).pos(.5f*ntf::vec2{w, h});
  });

  window.set_key_event([&](keycode code, auto, keystate state, auto) { 
    if (code == keycode::key_escape && state == keystate::press) {
      window.close();
    }
  });

  auto get_xy = [&](float lat, float lon) {
    auto max = bounds.second;
    auto min = bounds.first;
    float y_ratio = (lon - max.y)/(max.x);
    return window.size()*vec2{x_ratio, y_ratio};
  };

  ntf::log::debug("{} {} {} {}", bounds.first.x, bounds.first.y, bounds.second.x, bounds.second.y);
  // auto coord = get_xy(-24.872878, -65.462669);
  auto coord = get_xy(-24.875672, -65.456650);
  ntf::log::debug("{} {}", coord.x, coord.y);

  ntf::texture_data<gl::texture2d>::loader thing;
  auto cino = thing("res/cirno.png", ntf::tex_filter::nearest, ntf::tex_wrap::repeat);
  auto cino_transform = ntf::transform2d{}
    .scale(64);
    // .pos(coord);

  gl::framebuffer fbo{coso};
  manager.render_tiles(shader2, fbo);

  ntf::shogle_main_loop(window, 60,
    [&](double dt, double alpha) {
      imgui.start_frame();
      gl::clear_viewport(ntf::color3{.3f});
      shader.use();
      shader.set_uniform("model", transf.mat());
      shader.set_uniform("view", camera.view());
      shader.set_uniform("proj", camera.proj());
      shader.set_uniform("fb_sampler", (int)0);
      fbo.tex().bind_sampler(0);
      gl::draw_quad();

      shader2.use();
      shader2.set_uniform("model", cino_transform.mat());
      shader2.set_uniform("view", camera.view());
      shader2.set_uniform("proj", camera.proj());
      shader2.set_uniform("fb_sampler", (int)1);
      cino.bind_sampler(1);
      gl::draw_quad();
      // fbo.tex().bind_sampler((size_t)0);
      // cino.bind_sampler(0);
      imgui.end_frame();
    },
    [&]() {}
  );

  return 0;
}
