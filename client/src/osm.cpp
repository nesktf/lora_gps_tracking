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


// std::string tile_path(ntf::vec2 coord, float zoom) {
//   return fmt::format(osm_url, )
// }



} // namespace


namespace osm {

manager::manager(fs::path cache_path, coord box_min, coord box_max, std::size_t zoom) :
  _cache(cache_path),
  _box_min(box_min), _box_max(box_max), _zoom(zoom) {}

bool manager::prepare_tiles() {
  const auto min_tile = coord2tile(coord{_box_min.x, _box_min.y}, _zoom);
  const auto max_tile = coord2tile(coord{_box_max.x, _box_max.y}, _zoom);

  const std::size_t tile_size = 256;
  const ntf::ivec2 pixels {
    (max_tile.x - min_tile.x + 1)*tile_size,
    (max_tile.y - min_tile.y * 1)*tile_size
  };

  const std::size_t total = (1 + max_tile.x- min_tile.x)*(1 + max_tile.y - min_tile.y);
  ntf::log::debug("Fetching {} tiles!", total);
  ntf::log::debug("{} {} {} {}", max_tile.x, max_tile.y, min_tile.x, min_tile.y);


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
      // ntf::ivec2{
      //   tile_size*(tile_coord.x - min_tile.x),
      //   tile_size*(tile_coord.y - min_tile.y)
      // });
    }
  }
  _sz = (max_tile-min_tile);
  _sz *= 256;
  _min_tile = min_tile;
  return true;
}

std::pair<tile, tile> manager::tex_size() {
  return std::make_pair(_tiles.front().offset, _tiles.back().offset);
}

void manager::render_tiles(ntf::camera2d& cam, gl::shader_program& shader, gl::framebuffer& fb) {
  fb.bind(1024, 1024, [&, this]() {
    for (auto& tile : _tiles) {
      ntf::ivec2 pos = 256*(tile.offset - _min_tile);
      auto transf = ntf::transform2d{}
        .pos(pos)
        .scale(256);
      shader.use();
      shader.set_uniform("model", transf.mat());
      shader.set_uniform("view", cam.view());
      shader.set_uniform("proj", cam.proj());
      shader.set_uniform("fb_sampler", (int)0);
      tile.tex.bind_sampler(0);
      gl::draw_quad();
    }
  });
}

} // namespace osm
