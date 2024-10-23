#include "osm.hpp"
#include "shogle/scene/camera.hpp"
#include "shogle/scene/transform.hpp"

#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>

#include <shogle/core/log.hpp>
#include <shogle/assets/texture.hpp>
#include <shogle/render/gl/shader.hpp>

#include <fstream>

namespace {


bool download_thing(std::string_view url, std::string_view path) {
  try {
    std::ofstream stream{path.data(), std::ios::out | std::ios::binary};
    curlpp::Easy req;
    req.setOpt(curlpp::Options::Url{url.data()});
    req.setOpt(curlpp::Options::UserAgent{"Mozilla/5.0 (Windows NT 6.1; Win64; x64; rv:47.0) Gecko/20100101 Firefox/47.0"});
    req.setOpt(curlpp::Options::WriteStream{&stream});
    req.setOpt(curlpp::Options::WriteFunction([&](const char* p, size_t sz, size_t nmemb) {
      stream.write(p, sz*nmemb);
      return sz*nmemb;
    }));
    req.perform();
    return true;
  } 
  catch (curlpp::LogicError& e) {
    std::cout << e.what() << std::endl;
  }
  catch (curlpp::RuntimeError& e) {
    std::cout << e.what() << std::endl;
  }
  return false;

}

} // namespace


namespace osm {

map::map(fs::path cache_path, coord box_min, coord box_max, std::size_t zoom) :
  _cache(cache_path),
  _box_min(box_min), _box_max(box_max), _zoom(zoom) { prepare_tiles(); }

bool map::prepare_tiles() {

  shader_loader loader;
  auto vert = ntf::file_contents("res/shader/framebuffer.vs.glsl");
  auto frag = ntf::file_contents("res/shader/framebuffer.fs.glsl");
  _fbo_shader = loader(vert, frag);

  const auto min_tile = coord2tile(coord{_box_min.x, _box_min.y}, _zoom);
  const auto max_tile = coord2tile(coord{_box_max.x, _box_max.y}, _zoom);

  const std::size_t tile_size = 256;
  const ntf::ivec2 pixels {
    (max_tile.x - min_tile.x + 1)*tile_size,
    (max_tile.y - min_tile.y * 1)*tile_size
  };

  const std::size_t total = (1 + max_tile.x- min_tile.x)*(1 + max_tile.y - min_tile.y);
  ntf::log::debug("Fetching {} tiles!", total);

  for (int i = min_tile.x; i <= max_tile.x; ++i) {
    for (int j = min_tile.y; j <= max_tile.y; ++j) {
      const ntf::ivec2 tile_coord = {i, j};
      const std::string filename = fmt::format("osm-{}_{}_{}.png", _zoom, tile_coord.x, tile_coord.y);
      const fs::path file = _cache / filename;
      ntf::log::debug("{}", file.c_str());
      auto load_image = [&, this](fs::path path) -> ntf::gl_renderer::texture2d {
        ntf::texture_data<ntf::gl_renderer::texture2d>::loader loader;
        auto filter = ntf::tex_filter::nearest;
        auto wrap = ntf::tex_wrap::clamp_edge;

        if (fs::exists(path)) {
          return loader(path.c_str(), filter, wrap);
        }

        const std::string url = fmt::format("https://tile.openstreetmap.org/{}/{}/{}.png",
          _zoom, tile_coord.x, tile_coord.y
        );

        ntf::log::debug("{}", url);
        download_thing(url, path.c_str());
        return loader(path.c_str(), filter, wrap);
        return {};
      };

      _tiles.emplace_back(load_image(file), tile_coord);
    }
  }
  _sz = (max_tile-min_tile);
  _sz *= 256;
  _min_tile = min_tile;

  auto tsz = std::make_pair(_tiles.front().offset, _tiles.back().offset);
  _min_pos = osm::tile2coord(
    static_cast<ntf::vec2>(tsz.first)+ntf::vec2{0.5f}, _zoom
  );
  _max_pos = osm::tile2coord(
    static_cast<ntf::vec2>(tsz.second)+ntf::vec2{0.5f}, _zoom
  );

  _cam = ntf::camera2d{}
    .viewport(size())
    .znear(-10.f)
    .zfar(1.f)
    .pos(static_cast<ntf::vec2>(size())*.5f);
  _fbo = gl::framebuffer{size()};

  _transform = ntf::transform2d{}.scale(size());
  return true;
}

ntf::vec2 map::coord2pos(float lat, float lng) {
  auto sz = size();
  return ntf::vec2 {
    sz.y*(lng-_min_pos.y)/(_max_pos.y - _min_pos.y),
    sz.x*(lat-_min_pos.x)/(_max_pos.x - _min_pos.x),
  };
}

osm::map::map_object* map::add_object(gl::texture2d* tex, ntf::vec2 map_coords) {
  _objects.emplace_back(tex, map_coords, ntf::transform2d{}.pos(coord2pos(
    map_coords.x, map_coords.y
  )).scale(tex->dim()));
  return &_objects.back();
}

} // namespace osm
