#include "osm.hpp"

ntf::thread_pool threadpool;
std::atomic<bool> should_die{false};
std::atomic<bool> new_data{false};
bool nodemcu_connected{false};

osm::gps_data gps_data;
std::mutex gps_mtx;

const std::string_view nodemcu_url = "http://192.168.0.53:80";
std::chrono::high_resolution_clock::time_point last_update;

std::vector<std::pair<std::string, ntf::transform2d>> texts {
  {"conn:", ntf::transform2d{}.pos(25, 50)},
  {"avail:", ntf::transform2d{}.pos(25, 100)},
  {"lat:", ntf::transform2d{}.pos(25, 150)},
  {"lng:", ntf::transform2d{}.pos(25, 200)},
  {"sat:", ntf::transform2d{}.pos(25, 250)},
  {"last update:", ntf::transform2d{}.pos(25, 900)},
};

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

  threadpool.enqueue([&]() {
    using namespace std::chrono_literals;
    using nlohmann::json;

    std::string json_string;
    while (!should_die.load()) {
      if (!osm::download_string(nodemcu_url, json_string)) {
        std::unique_lock lock{gps_mtx};
        ntf::log::error("Failed to connect to NodeMCU");
        gps_data.available = false;
        nodemcu_connected = false;
        new_data.store(true);
        std::this_thread::sleep_for(5s);
        continue;
      }

      try {
        json contents = json::parse(json_string);
        std::unique_lock lock{gps_mtx};
        gps_data.available = static_cast<bool>(contents["available"].get<int>());
        gps_data.rssi = contents["rssi"].get<int>();
        gps_data.time = contents["time"].get<uint32_t>();
        gps_data.sat_c = contents["sat_count"].get<uint32_t>();
        gps_data.lat = contents["lat"].get<float>();
        gps_data.lng = contents["lng"].get<float>();
        nodemcu_connected = true;
      }
      catch (json::exception& e) {
        std::unique_lock lock{gps_mtx};
        ntf::log::error("Failed to parse GPS json {}", e.what());
        gps_data.available = false;
        new_data.store(true);
        std::this_thread::sleep_for(5s);
        continue;
      }
      
      last_update = std::chrono::high_resolution_clock::now();
      ntf::log::info("GPS data updated {}", last_update);
      new_data.store(true);
      std::this_thread::sleep_for(5s);
    };
  });


  shader_loader sloader;
  auto tile_shader = sloader(
    ntf::file_contents("res/shader/tile.vs.glsl"), 
    ntf::file_contents("res/shader/tile.fs.glsl")
  );
  auto font_shader = sloader(
    ntf::file_contents("res/shader/font.vs.glsl"),
    ntf::file_contents("res/shader/font.fs.glsl")
  );

  texture_loader tloader;
  auto cino = tloader("res/marker.png", ntf::tex_filter::nearest, ntf::tex_wrap::repeat);


  font_loader floader;
  auto cousine = floader("res/font/CousineNerdFont-Regular.ttf");

  auto camera = ntf::camera2d{}.pos((ntf::vec2)window.size()*.5f)
    .viewport(window.size()).zfar(1.f).znear(-10.f);


  auto draw_text = [&](ntf::transform2d& transf, const std::string& text) {
    font_shader.use();
    font_shader.set_uniform("proj", camera.proj());
    font_shader.set_uniform("model", transf.mat());
    font_shader.set_uniform("text_color", ntf::color4{0.f, 0.f, 0.f, 1.f});
    font_shader.set_uniform("tex", 0);
    cousine.draw_text(ntf::vec2{0.f}, 1.f, text);
  };

  // map.add_object(&cino, ntf::vec2{-24.872878, -65.462669});
  // map.add_object(&cino, ntf::vec2{-24.875672, -65.456650});
  // map.add_object(&cino, ntf::vec2{-24.873745, -65.457208});

  map.transform().pos((ntf::vec2)window.size()*.5f);

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

  auto render = [&](double, double) {
    imgui.start_frame();
    gl::clear_viewport(ntf::color3{.3f});

    map.render(window.size(),camera, [&](ntf::transform2d& obj_transf, ntf::camera2d& map_cam, 
                                         const int sampler, const bool hidden) {
      if (hidden) {
        return;
      }
      tile_shader.use();
      tile_shader.set_uniform("model", obj_transf.mat());
      tile_shader.set_uniform("view", map_cam.view());
      tile_shader.set_uniform("proj", map_cam.proj());
      tile_shader.set_uniform("fb_sampler", sampler);
      gl::draw_quad();
    });

    for (auto& [text, transf] : texts) {
      draw_text(transf, text);
    }

    imgui.end_frame();
  };

  osm::map::map_object* marker{nullptr};

  auto tick = [&]() {
    if (!new_data.load()) {
      return;
    }
    texts[0].first = fmt::format("conn: {}", nodemcu_connected ? "true" : "false");
    texts[1].first = fmt::format("avail: {}", gps_data.available ? "true" : "false");
    texts[2].first = fmt::format("lat: {}", gps_data.lat);
    texts[3].first = fmt::format("lng: {}", gps_data.lng);
    texts[4].first = fmt::format("sat: {}", gps_data.sat_c);
    texts[5].first = fmt::format("last update: {}", last_update);

    new_data.store(false);

    if (!gps_data.available) {
      if (marker) {
        marker->hidden = true;
      }
      return;
    }

    if (!marker) {
      marker = map.add_object(&cino, ntf::vec2{gps_data.lat, gps_data.lng});

      marker->transform.scale(ntf::vec2{32, 32});
      return;
    }

    ntf::log::debug("A");
    marker->hidden = false;
    map.update_object(marker, ntf::vec2{gps_data.lat, gps_data.lng});
  };

  ntf::shogle_main_loop(window, 60, render, tick);
  should_die.store(true);

  return 0;
}
