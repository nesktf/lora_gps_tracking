#pragma once

#include <shogle/render/gl.hpp>
#include <shogle/render/glfw.hpp>

#include <shogle/scene/transform.hpp>
#include <shogle/scene/camera.hpp>
#include <shogle/scene/transform.hpp>

#include <shogle/render/gl/framebuffer.hpp>
#include <shogle/render/gl/texture.hpp>
#include <shogle/render/gl/shader.hpp>
#include <shogle/render/gl/font.hpp>

#include <shogle/engine.hpp>

#include <shogle/assets/texture.hpp>
#include <shogle/assets/font.hpp>

#include <shogle/core/log.hpp>
#include <shogle/core/threadpool.hpp>

#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <atomic>
#include <fstream>
#include <cstdint>

using gl = ntf::gl_renderer;
using glfw = ntf::glfw;
using keycode = glfw::keycode;
using keystate = glfw::keystate;
using logger = ntf::log;
namespace fs = std::filesystem;

using texture_loader = ntf::texture_data<gl::texture2d>::loader;
using shader_loader = gl::shader_program::loader;
using font_loader = ntf::font_data<gl::font>::loader;

namespace osm {

using coord = glm::dvec2;
using tile = ntf::ivec2;

struct gps_data {
  float lat{}, lng{};
  uint32_t sat_c{}, time{};
  int rssi{0};
  bool available{false};
};

class map {
public:
  struct tile_texture {
    gl::texture2d tex;
    tile offset;
    // coord coord;
  };

  struct map_object {
    gl::texture2d* tex;
    ntf::vec2 map_coords;
    ntf::transform2d transform;
    bool hidden{false};
  };
public:
  map(fs::path cache_path, coord box_min, coord box_max, std::size_t zoom);

public:
  bool prepare_tiles();
  ntf::ivec2 size() const { return _sz; }

  map_object* add_object(gl::texture2d* tex, ntf::vec2 map_coords);
  void update_object(map_object* obj, ntf::vec2 coords);
  ntf::vec2 coord2pos(float lat, float lng);

  template<typename Fun>
  void render(ntf::ivec2 win_size, ntf::camera2d& cam, Fun&& render_fun) {
    const int sampler = 0;
    _fbo.bind(win_size, [&, this]() {
      for (auto& tile : _tiles) {
        ntf::ivec2 pos = 256*(tile.offset - _min_tile);
        auto transf = ntf::transform2d{}
          .pos(pos)
          .scale(256);

        tile.tex.bind_sampler((std::size_t)sampler);
        render_fun(transf, _cam, sampler, false);
      }

      for (auto& obj : _objects) {
        obj.tex->bind_sampler((std::size_t)sampler);
        render_fun(obj.transform, _cam, sampler, obj.hidden);
      }
    });
    _fbo_shader.use();
    _fbo_shader.set_uniform("model", _transform.mat());
    _fbo_shader.set_uniform("view", cam.view());
    _fbo_shader.set_uniform("proj", cam.proj());
    _fbo_shader.set_uniform("fb_sampler", (int)0);
    _fbo.tex().bind_sampler(0);
    gl::draw_quad();
  }

  ntf::transform2d& transform() { return _transform; }

private:
  fs::path _cache;
  coord _box_min, _box_max;
  ntf::ivec2 _sz;
  ntf::ivec2 _min_tile;
  std::size_t _zoom;
  std::vector<tile_texture> _tiles;

  gl::framebuffer _fbo;
  ntf::camera2d _cam;

  ntf::vec2 _min_pos, _max_pos;

  std::vector<map_object> _objects;
  gl::shader_program _fbo_shader;
  ntf::transform2d _transform;
};


inline ntf::ivec2 coord2tile(osm::coord coord, std::size_t zoom) {
  float lat = ntf::rad(coord.x);
  int n = 1 << zoom;
  int xtile = static_cast<int>(n*(coord.y + 180.f) / 360.f);
  int ytile = static_cast<int>(
    (1.f - std::log(std::tan(lat) + (1/std::cos(lat))) / M_PIf)*.5f*n
  );
  return {xtile, ytile};
}

inline osm::coord tile2coord(ntf::vec2 tile, std::size_t zoom) {
  int n = 1 << zoom;
  float lon = (tile.x/static_cast<float>(n))*360.f - 180.f;
  float lat = ntf::deg(std::atan(std::sinh(M_PIf*(1-2*tile.y / static_cast<float>(n)))));
  return {lat, lon};
}


bool download_thing(std::string_view url, std::string_view path);
bool download_string(std::string_view url, std::string& contents);

} // namespace osm
