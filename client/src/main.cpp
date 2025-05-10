#include "renderer.hpp"
#include "osm.hpp"
#include "marker.hpp"

static gps_coord map_min{-24.737526, -65.394627}; // top left
static gps_coord map_max{-24.744542, -65.387117}; // bottom right
static uint32 map_zoom = 19u;

static const char* cache_dir = "tile_cache/";
static const char* nodemcu_url = "http://192.168.89.53:80";

struct map_object {
  size_t tex;
  ntf::transform2d<float> transform;
};

int main(int argc, const char* argv[]) {
  logger::set_level(ntf::log_level::verbose);
  if (argc >= 2) {
    cache_dir = argv[1];
  }
  if (argc >= 3) {
    nodemcu_url = argv[2];
  }
  logger::info("[main] Tile cache dir: \"{}\"", cache_dir);
  logger::info("[main] NodeMCU API url: \"{}\"", nodemcu_url);

  {
    auto vert_src = ntf::file_contents("res/shader/tile.vs.glsl").value(); 
    auto frag_src = ntf::file_contents("res/shader/tile.fs.glsl").value();
    auto font_atlas = ntf::load_font_atlas<char>("res/font/CousineNerdFont-Regular.ttf").value();
    render_ctx::construct(vert_src, frag_src, std::move(font_atlas), {1280, 720});
  }
  auto& render = render_ctx::instance();
  render.cam_pos(1280.f, -1280.f);

  auto sdf = gps_marker::make_marker(10.f, 35.f);
  // auto sdf2 = map_shape::make_shape(map_shape::S_TRIANGLE, 20.f, color4{0.f, 1.f, 1.f, 1.f});
  auto sdf3 = map_shape::make_shape(map_shape::S_TRIANGLE, 7.f, color4{1.f, 0.f, 0.f, 1.f});
  auto sdf4 = map_shape::make_shape(map_shape::S_SQUARE, 6.f, color4{1.f, 0.f, 0.f, 1.f});
  // sdf2.set_outline_color(color4{1.f, 0.f, 0.f, 1.f});
  // sdf2.set_pos({1280, -1280});
  std::vector<map_shape> checkpoints;
  size_t selected = 0u;

  // bezier_thing bez{};

  vec2 mouse_pos{};
  render.window().set_key_press_callback([&](auto& win, const ntf::win_key_data& key) {
    auto cam_pos = render.cam_pos();
    if (key.action == ntf::win_action::press) {
      if (key.key == ntf::win_key::escape) {
        win.close();
      }
      if (key.key == ntf::win_key::up) {
        cam_pos.y += 256.f;
      } else if (key.key == ntf::win_key::down){
        cam_pos.y -= 256.f;
      }
      if (key.key == ntf::win_key::left) {
        cam_pos.x -= 256.f;
      } else if (key.key == ntf::win_key::right) {
        cam_pos.x += 256.f;
      }
      if (key.key == ntf::win_key::backspace) {
        checkpoints.clear();
        selected = 0u;
      }
    }
    render.cam_pos(cam_pos.x, cam_pos.y);
  }).set_viewport_callback([&](auto&, const ntf::extent2d& ext) {
    render.update_viewport(ext.x, ext.y);
  }).set_cursor_pos_callback([&](auto&, dvec2 pos) {
      mouse_pos.x = pos.x;
      mouse_pos.y = -pos.y;
    // mouse_pos = render.raycast(pos.x, pos.y);
  });

  osm_map map{cache_dir};
  std::vector<map_object> objs;
  gps_coord cino_coord{-24.741087, -65.389729};
  const auto tileset = map.load_tiles(map_min, map_max, map_zoom);
  objs.reserve(tileset.tiles().size()+1u);

  for (const auto& tile : tileset.tiles()) {
    auto tile_transf = ntf::transform2d<float>{}
      .pos(tile.pos.x, tile.pos.y).scale(tileset.TILE_SIZE);
    logger::debug(" => {} {}", tile_transf.pos_x(), tile_transf.pos_y());
    objs.emplace_back(render.make_texture(tile.image), tile_transf);
  }
  auto marker_data = ntf::load_image<ntf::uint8>("res/cirno.png").value();
  // cino_coord = tileset.max_coord();
  // const auto cino_pos = tileset.pos_from_coord(cino_coord);
  // logger::debug("CINO: {} {}", cino_pos.x, cino_pos.y);
  // objs.emplace_back(render.make_texture(marker_data), ntf::transform2d<float>{}
  //   .pos(cino_pos).scale(64.f));
  // sdf.set_pos(cino_pos);
  // render.cam_pos(cino_pos.x, cino_pos.y);

  render.window().set_button_press_callback([&](auto&, const ntf::win_button_data& butt) {
    if (butt.action == ntf::win_action::press) {
      if (butt.button == ntf::win_button::m1) {
        auto coso = tileset.coord_from_pos(mouse_pos);
        logger::debug("LCLICK! {}, {}", coso.x, coso.y);
      }
      if (butt.button == ntf::win_button::m2) {
        auto wp = render.raycast(mouse_pos.x, -mouse_pos.y);
        checkpoints.emplace_back(map_shape::make_shape(map_shape::S_DIAMOND, 7.f,
                                                       color4{0.f, 1.f, 0.f, .75f}));
        checkpoints.back().set_pos(wp);
        checkpoints.back().set_outline_color(color4{1.f, 0.f, 0.f, .75f});
      }
    }
  });


  auto query = map.query_gps();
  vec2 last_mouse_pos{};
  float angle{};
  vec2 dir{};
  render.start_loop(60u, ntf::overload{
    // Update call
    [&](uint32 ups) {
      const float dt = 1/static_cast<float>(ups);
      auto cam_pos = render.cam_pos();
      // auto& cino = objs.back();
      const auto mouse_world = render.raycast(mouse_pos.x, -mouse_pos.y);
      sdf.set_pos(mouse_world);

      const auto mouse_delta = (mouse_pos - last_mouse_pos)*dt;


      // cino.transform.pos(mouse_world + dir*100.f);
      // cino.transform.rot(0.f, 0.f, angle);

      if (render.window().poll_button(ntf::win_button::m1) == ntf::win_action::press) {
        cam_pos += mouse_delta*-60.f;
        cam_pos.x = glm::clamp(cam_pos.x, 850.f, 3000.f);
        cam_pos.y = glm::clamp(cam_pos.y, -3000.f, -450.f);
        render.cam_pos(cam_pos.x, cam_pos.y);
      }

      if (!checkpoints.empty()) {
        auto& obj = checkpoints[selected];
        if (!ntf::collision2d(mouse_world, 5.f, obj.pos(), obj.size())) {
          obj.set_outline_width(2.f);
        } else {
          selected = (selected+1u)% checkpoints.size();
          obj.set_outline_width(0.f);
        }
        // logger::debug("{}", selected);
        dir = glm::normalize(obj.pos()-sdf.pos());
        const vec2 up{0.f, 1.f};

        angle = glm::acos(glm::dot(dir, up));
        if (dir.x >= 0.f){
          angle *= -1.f;
        }
        sdf3.set_pos(mouse_world + dir*25.f);
        sdf3.set_rot(angle+M_PIf);
        sdf4.set_pos(mouse_world + dir*15.f);
        sdf4.set_rot(angle+M_PIf);
      }

      last_mouse_pos = mouse_pos;
    },

    // Render call
    [&]([[maybe_unused]] double dt, [[maybe_unused]] double alpha) {
      render.start_render();

      // auto& cino = objs.back().transform;
      auto cam_pos = tileset.coord_from_pos(render.cam_pos());
      auto ppos = tileset.coord_from_pos(sdf.pos());
      render.render_text(20.f, 200.f, 1.f, "pos {:.7f} {:.7f}",
                         ppos.x, ppos.y);
      // render.render_string(20.f, 600.f, 1.f, query.info);
      // render.render_text(100.f, 100.f, 1.f, "~ze");
      render.render_text(20.f, 150.f, 1.f, "map_pos {:.7f},{:.7f}", cam_pos.x, cam_pos.y);
      // render.render_text(100.f, 250.f, 1.f, "cino_coord {:.7f},{:.7f}",
      //                    cino_coord.x, cino_coord.y);
      // render.render_text(100.f, 300.f, 1.f, "cino_pos {:.2f},{:.2f}", cino.pos_x(), cino.pos_y());
      for (auto& obj : objs) {
        render.render_texture(obj.tex, obj.transform.world());
      }
      for (auto& check : checkpoints) {
        render.render_thing(check);
      }
      render.render_thing(sdf);
      if (!checkpoints.empty()) {{
        auto pos = tileset.coord_from_pos(checkpoints[selected].pos());
        render.render_text(20.f, 100.f, 1.f, "check_pos {:.7f},{:.7f}",
                           pos.x, pos.y);
        render.render_text(20.f, 50.f, 1.f, "angle {:.2f},{:.2f} ({:.2f} deg)",
                           dir.x, dir.y, glm::degrees(angle+M_PIf));
        render.render_thing(sdf4);
        render.render_thing(sdf3);
      }}

      // render.render_thing(sdf2);

      // render.render_text(mouse_pos.x-180.f, 50.f+mouse_pos.y+render.viewport().y, 1.f,
      //                    "BAKA DETECTED");
      // render.render_thing(bez);

      render.end_render();
    },
  });
  render_ctx::destroy();

// ntf::thread_pool threadpool;
// std::atomic<bool> should_die{false};
// std::atomic<bool> new_data{false};
// bool nodemcu_connected{false};
//
// osm::gps_data gps_data;
// std::mutex gps_mtx;
//
// std::chrono::high_resolution_clock::time_point last_update;
//
// std::vector<std::pair<std::string, ntf::transform2d>> texts {
//   {"conn:", ntf::transform2d{}.pos(25, -250)},
//   {"avail:", ntf::transform2d{}.pos(25, -200)},
//   {"lat:", ntf::transform2d{}.pos(25, -150)},
//   {"lng:", ntf::transform2d{}.pos(25, -100)},
//   {"sat:", ntf::transform2d{}.pos(25, -50)},
//   {"last update:", ntf::transform2d{}.pos(25, 50)},
// };
//
  // osm::map map{"tile_cache/",
  //   osm::coord{-24.737526, -65.394627}, // top left
  //   osm::coord{-24.744542, -65.387117}, // bottom right
  //   17
  // };
  //
  // threadpool.enqueue([&]() {
  //   using namespace std::chrono_literals;
  //   using nlohmann::json;
  //
  //   std::string json_string;
  //   while (!should_die.load()) {
  //     if (!osm::download_string(nodemcu_url, json_string)) {
  //       std::unique_lock lock{gps_mtx};
  //       ntf::log::error("Failed to connect to NodeMCU");
  //       gps_data.available = false;
  //       nodemcu_connected = false;
  //       new_data.store(true);
  //       std::this_thread::sleep_for(5s);
  //       continue;
  //     }
  //
  //     try {
  //       json contents = json::parse(json_string);
  //       std::unique_lock lock{gps_mtx};
  //       gps_data.available = static_cast<bool>(contents["available"].get<int>());
  //       gps_data.rssi = contents["rssi"].get<int>();
  //       gps_data.time = contents["time"].get<uint32_t>();
  //       gps_data.sat_c = contents["sat_count"].get<uint32_t>();
  //       gps_data.lat = contents["lat"].get<float>();
  //       gps_data.lng = contents["lng"].get<float>();
  //       nodemcu_connected = true;
  //     }
  //     catch (json::exception& e) {
  //       std::unique_lock lock{gps_mtx};
  //       ntf::log::error("Failed to parse GPS json {}", e.what());
  //       gps_data.available = false;
  //       new_data.store(true);
  //       std::this_thread::sleep_for(5s);
  //       continue;
  //     }
  //     
  //     last_update = std::chrono::high_resolution_clock::now();
  //     ntf::log::info("GPS data updated {}", last_update);
  //     new_data.store(true);
  //     std::this_thread::sleep_for(5s);
  //   };
  // });
  //
  //
  // shader_loader sloader;
  // auto tile_shader = sloader(
  //
  // );
  // auto font_shader = sloader(
  //   ntf::file_contents("res/shader/font.vs.glsl"),
  //   ntf::file_contents("res/shader/font.fs.glsl")
  // );
  //
  // texture_loader tloader;
  // auto cino = tloader("res/marker.png", ntf::tex_filter::nearest, ntf::tex_wrap::repeat);
  //
  //
  // font_loader floader;
  // auto cousine = floader("res/font/CousineNerdFont-Regular.ttf");
  //
  // auto camera = ntf::camera2d{}.pos((ntf::vec2)window.size()*.5f)
  //   .viewport(window.size()).zfar(1.f).znear(-10.f);
  //
  //
  // auto draw_text = [&](ntf::transform2d& transf, const std::string& text) {
  //   font_shader.use();
  //   font_shader.set_uniform("proj", camera.proj());
  //   font_shader.set_uniform("model", transf.mat());
  //   font_shader.set_uniform("text_color", ntf::color4{0.f, 0.f, 0.f, 1.f});
  //   font_shader.set_uniform("tex", 0);
  //   cousine.draw_text(ntf::vec2{0.f}, 1.f, text);
  // };
  //
  //
  // auto sz = (ntf::vec2)map.size();
  // map.transform().pos((ntf::vec2)window.size()*.5f+ntf::vec2{256,256}).scale(sz*2.f);
  //
  // window.set_viewport_event([&](std::size_t w, std::size_t h) {
  //   gl::set_viewport(w, h);
  //   camera.viewport(w, h).pos(.5f*ntf::vec2{w, h});
  //   map.transform().pos((ntf::vec2)window.size()*.5f);
  // });
  //
  // window.set_key_event([&](keycode code, auto, keystate state, auto) { 
  //   if (code == keycode::key_escape && state == keystate::press) {
  //     window.close();
  //   }
  //
  //   const float mov = 128.f;
  //   auto pos = map.transform().pos();
  //   if (code == keycode::key_left && state == keystate::press) {
  //     pos.x += mov;
  //   } else if (code == keycode::key_right && state == keystate::press) {
  //     pos.x -= mov;
  //   }
  //
  //   if (code == keycode::key_up && state == keystate::press) {
  //     pos.y += mov;
  //   } else if (code == keycode::key_down && state == keystate::press) {
  //     pos.y -= mov;
  //   }
  //
  //   map.transform().pos(pos);
  // });
  //
  // auto render = [&](double, double) {
  //   imgui.start_frame();
  //   gl::clear_viewport(ntf::color3{.3f});
  //
  //   map.render(window.size(),camera, [&](ntf::transform2d& obj_transf, ntf::camera2d& map_cam, 
  //                                        const int sampler, const bool hidden) {
  //     if (hidden) {
  //       return;
  //     }
  //     tile_shader.use();
  //     tile_shader.set_uniform("model", obj_transf.mat());
  //     tile_shader.set_uniform("view", map_cam.view());
  //     tile_shader.set_uniform("proj", map_cam.proj());
  //     tile_shader.set_uniform("fb_sampler", sampler);
  //     gl::draw_quad();
  //   });
  //
  //   auto vp = window.size();
  //   for (auto& [text, transf] : texts) {
  //     auto ntransf = transf;
  //     if (ntransf.pos().y < 0) {
  //       ntransf.pos(ntransf.pos().x, vp.y+ntransf.pos().y);
  //     }
  //     draw_text(ntransf, text);
  //   }
  //
  //   imgui.end_frame();
  // };
  //
  // osm::map::map_object* marker{nullptr};
  //
  // auto tick = [&]() {
  //   if (!new_data.load()) {
  //     return;
  //   }
  //   texts[0].first = fmt::format("conn: {}", nodemcu_connected ? "true" : "false");
  //   texts[1].first = fmt::format("avail: {}", gps_data.available ? "true" : "false");
  //   texts[2].first = fmt::format("lat: {}", gps_data.lat);
  //   texts[3].first = fmt::format("lng: {}", gps_data.lng);
  //   texts[4].first = fmt::format("sat: {}", gps_data.sat_c);
  //   texts[5].first = fmt::format("last update: {}", last_update);
  //
  //   new_data.store(false);
  //
  //   if (!gps_data.available) {
  //     if (marker) {
  //       marker->hidden = true;
  //     }
  //     return;
  //   }
  //
  //   if (!marker) {
  //     marker = map.add_object(&cino, ntf::vec2{gps_data.lat, gps_data.lng});
  //
  //     marker->transform.scale(ntf::vec2{16, 16});
  //     return;
  //   }
  //
  //   marker->hidden = false;
  //   map.update_object(marker, ntf::vec2{gps_data.lat, gps_data.lng});
  // };
  //
  // ntf::shogle_main_loop(window, 60, render, tick);
  // should_die.store(true);

  return 0;
}
