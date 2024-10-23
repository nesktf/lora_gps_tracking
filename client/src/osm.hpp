#pragma once

#include "shogle/scene/camera.hpp"
#include <shogle/render/gl/framebuffer.hpp>
#include <shogle/render/gl/texture.hpp>

#include <filesystem>

namespace osm {

using gl = ntf::gl_renderer;
namespace fs = std::filesystem;

using coord = glm::dvec2;
using tile = ntf::ivec2;

class manager {
public:
  struct tile_texture {
    gl::texture2d tex;
    tile offset;
    // coord coord;
  };
public:
  manager(fs::path cache_path, coord box_min, coord box_max, std::size_t zoom);

public:
  bool prepare_tiles();
  [[nodiscard]] std::pair<tile, tile> tex_size();
  // [[nodiscard]] std::pair<coord, coord> get_tile_coords();
  void render_tiles(ntf::camera2d& cam, gl::shader_program& shader, gl::framebuffer& fb);
  ntf::ivec2 size() const { return _sz; }

private:
  fs::path _cache;
  coord _box_min, _box_max;
  ntf::ivec2 _sz;
  ntf::ivec2 _min_tile;
  std::size_t _zoom;
  std::vector<tile_texture> _tiles;
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

} // namespace osm
