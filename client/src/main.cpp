#include "osm.hpp"

int main() {
  logger::set_level(logger::level::verbose);

  auto glfw = ntf::glfw::init();
  glfw::set_swap_interval(0);

  glfw::window<ntf::gl_renderer> window{1024, 1024, "test"};
  auto imgui = ntf::imgui::init(window, ntf::imgui::glfw_gl3_impl{});

  gl::set_blending(true);


  osm::map map{"tile_cache/",
    osm::coord{-24.87034, -65.46616}, // top left
    osm::coord{-24.87910, -65.45532}, // bottom right
    17
  };


  shader_loader loader;

  auto shader2 = loader(
    ntf::file_contents("res/shader/tile.vs.glsl"), 
    ntf::file_contents("res/shader/tile.fs.glsl")
  );

  auto camera = ntf::camera2d{}.pos((ntf::vec2)window.size()*.5f)
    .viewport(window.size()).zfar(1.f).znear(-10.f);

  auto vp = camera.viewport();

  window.set_viewport_event([&](std::size_t w, std::size_t h) {
    gl::set_viewport(w, h);
    camera.viewport(w, h).pos(.5f*ntf::vec2{w, h});
  map.transform().pos((ntf::vec2)window.size()*.5f);
  });

  window.set_key_event([&](keycode code, auto, keystate state, auto) { 
    if (code == keycode::key_escape && state == keystate::press) {
      window.close();
    }
  });

  texture_loader thing;
  auto cino = thing("res/cirno.png", ntf::tex_filter::nearest, ntf::tex_wrap::repeat);

  map.add_object(&cino, ntf::vec2{-24.872878, -65.462669});
  map.add_object(&cino, ntf::vec2{-24.875672, -65.456650});
  map.add_object(&cino, ntf::vec2{-24.873745, -65.457208});
  map.transform().pos((ntf::vec2)window.size()*.5f);

  auto render = [&](double, double) {
    imgui.start_frame();
    gl::clear_viewport(ntf::color3{.3f});

    map.render(window.size(),camera, [&](ntf::transform2d& obj_transf, ntf::camera2d& map_cam, 
                                         const int sampler) {
      shader2.use();
      shader2.set_uniform("model", obj_transf.mat());
      shader2.set_uniform("view", map_cam.view());
      shader2.set_uniform("proj", map_cam.proj());
      shader2.set_uniform("fb_sampler", sampler);
      gl::draw_quad();
    });
    imgui.end_frame();
  };

  auto tick = [&]() {

  };

  ntf::shogle_main_loop(window, 60, render, tick);

  return 0;
}
