#pragma once

#include "./renderer.hpp"

#include <filesystem>
#include <atomic>
#include <fstream>
#include <cstdint>

namespace fs = std::filesystem;

using chrono_clock = std::chrono::high_resolution_clock;

using gps_coord = dvec2;
using tile_coord = ivec2;

class osm_tileset {
public:
  static constexpr uint32 TILE_SIZE = 256u; // pixels

  struct tile_t {
    ntf::image_data image;
    vec2 pos;
  };

public:
  osm_tileset(std::vector<tile_t>&& tiles, vec2 min_coord, vec2 max_coord, vec2 size) noexcept;

public:
  vec2 pos_from_coord(gps_coord coord) const;

  gps_coord min_coord() const { return _min_coord; }
  gps_coord max_coord() const { return _max_coord;}

public:
  ntf::cspan<tile_t> tiles() const { return {_tiles.data(), _tiles.size()}; }

private:
  std::vector<tile_t> _tiles;
  gps_coord _min_coord, _max_coord;
  vec2 _size;
};

class osm_map {
public:
  struct gps_data {
    float lat, lng;
    uint32 sat_c, time;
    int rssi;
    bool available;
    chrono_clock::time_point last_update;
  };

  struct gps_query {
    vec2 pos;
    float radius;
    std::string info;
  };

public:
  osm_map(fs::path cache_path);

public:
  osm_tileset load_tiles(gps_coord min_coord, gps_coord max_coord, uint32 zoom);

public:
  gps_query query_gps();

private:
  fs::path _cache;
  gps_data _gps;
};

// class osm_map {
// public:
//   struct tile_data {
//     ntf::image_data image;
//     gl::texture2d tex;
//     tile offset;
//     // coord coord;
//   };
//   //
//   // struct map_object {
//   //   gl::texture2d* tex;
//   //   ntf::vec2 map_coords;
//   //   ntf::transform2d transform;
//   //   bool hidden{false};
//   // };
//
// public:
//   osm_map(fs::path cache_path, coord box_min, coord box_max, std::size_t zoom);
//
// public:
//   bool prepare_tiles();
//   std::vector<ntf::image_data> load_tiles();
//
//   ntf::ivec2 size() const { return _sz; }
//
//   map_object* add_object(gl::texture2d* tex, ntf::vec2 map_coords);
//   void update_object(map_object* obj, ntf::vec2 coords);
//   ntf::vec2 coord2pos(float lat, float lng);
//
//   template<typename Fun>
//   void render(ntf::ivec2 win_size, ntf::camera2d& cam, Fun&& render_fun) {
//     const int sampler = 0;
//     _fbo.bind(win_size, [&, this]() {
//       for (auto& tile : _tiles) {
//         ntf::ivec2 pos = 256*(tile.offset - _min_tile);
//         auto transf = ntf::transform2d{}
//           .pos(pos)
//           .scale(256);
//
//         tile.tex.bind_sampler((std::size_t)sampler);
//         render_fun(transf, _cam, sampler, false);
//       }
//
//       for (auto& obj : _objects) {
//         obj.tex->bind_sampler((std::size_t)sampler);
//         render_fun(obj.transform, _cam, sampler, obj.hidden);
//       }
//     });
//     _fbo_shader.use();
//     _fbo_shader.set_uniform("model", _transform.mat());
//     _fbo_shader.set_uniform("view", cam.view());
//     _fbo_shader.set_uniform("proj", cam.proj());
//     _fbo_shader.set_uniform("fb_sampler", (int)0);
//     _fbo.tex().bind_sampler(0);
//     gl::draw_quad();
//   }
//
//   ntf::transform2d& transform() { return _transform; }
//
// private:
//   fs::path _cache;
//   coord _box_min, _box_max;
//   ntf::ivec2 _sz;
//   ntf::ivec2 _min_tile;
//   std::size_t _zoom;
//   std::vector<tile_texture> _tiles;
//
//   gl::framebuffer _fbo;
//   ntf::camera2d _cam;
//
//   ntf::vec2 _min_pos, _max_pos;
//
//   std::vector<map_object> _objects;
//   gl::shader_program _fbo_shader;
//   ntf::transform2d _transform;
// };
//

//
//

