#include "osm.hpp"

#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>

#include <nlohmann/json.hpp>

static const char* CURL_UA =
  "Mozilla/5.0 (Windows NT 6.1; Win64; x64; rv:47.0) Gecko/20100101 Firefox/47.0";

static std::string format_osm_url(ivec2 tile, uint32 zoom) {
  return fmt::format("https://tile.openstreetmap.org/{}/{}/{}.png",
                     zoom, tile.x, tile.y);
}

namespace curlopts = curlpp::Options;

// Synchronous
static bool download_to_file(std::string_view url, const fs::path& path) {
  try {
    std::ofstream stream{path.c_str(), std::ios::out | std::ios::binary};
    curlpp::Easy req;
    req.setOpt(curlopts::Url{url.data()});
    req.setOpt(curlopts::UserAgent{CURL_UA});
    req.setOpt(curlopts::WriteStream{&stream});
    req.setOpt(curlopts::WriteFunction([&](const char* p, size_t sz, size_t nmemb) {
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

// Synchronous
static bool download_string(std::string_view url, std::string& contents) {
  std::string out;
  try {
    curlpp::Easy req;
    req.setOpt(curlopts::Url{url.data()});
    req.setOpt(curlopts::Timeout{1});
    req.setOpt(curlopts::UserAgent{CURL_UA});
    req.setOpt(curlopts::WriteFunction{[&](const char* p, size_t sz, size_t nmemb) {
      out.append(p, sz*nmemb);
      return sz*nmemb;
    }});
    req.perform();
    contents = std::move(out);
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

static ivec2 coord2tile(dvec2 coord, uint32 zoom) {
  const double lat = glm::radians(coord.x);
  const double n = static_cast<double>(int{1 << zoom});
  const int xtile = static_cast<int>(n*(coord.y + 180.) / 360.);
  const int ytile = static_cast<int>((1.-std::log(std::tan(lat) + (1./std::cos(lat)))/M_PI)*.5*n);
  return {xtile, ytile};
}

static dvec2 tile2coord(ivec2 tile, uint32 zoom) {
  const double n = static_cast<double>(int{1 << zoom});
  const double lon = (tile.x/n)*360. - 180.;
  const double lat = glm::degrees(std::atan(std::sinh(M_PI*(1-2*tile.y / n))));
  return {lat, lon};
}

osm_map::osm_map(fs::path cache_path) :
  _cache{cache_path}, _gps{} {}

auto osm_map::load_tiles(
  dvec2 box_min, dvec2 box_max, uint32 zoom, uint32 tile_size
) -> std::vector<tile_data> {
  const auto min_tile = coord2tile(box_min, zoom);
  const auto max_tile = coord2tile(box_max, zoom);
  logger::debug("[osm_map] min: ({} {}), max: ({} {})",
                min_tile.x, min_tile.y, max_tile.x, max_tile.y);

  const uint32 tile_count = (1 + max_tile.x - min_tile.x)*(1 + max_tile.y - min_tile.y);
  logger::info("[osm_map] Fetching {} tiles!", tile_count);
  // const ivec2 pixels {
  //   (max_tile.x - min_tile.x + 1)*tile_size,
  //   (max_tile.y - min_tile.y * 1)*tile_size
  // };
  const auto coord2pos = [&](dvec2 coord) -> vec2 {
    const float lat = coord.x;
    const float lng = coord.y;
    ivec2 map_sz = max_tile-min_tile;
    map_sz *= tile_size;
    const auto min_pos = tile2coord(min_tile, zoom);
    const auto max_pos = tile2coord(max_tile, zoom);

    const float x = map_sz.y*(lng-min_pos.y)/(max_pos.y-min_pos.y);
    const float y = map_sz.x*(lat-min_pos.x)/(max_pos.x-min_pos.x);
    logger::debug(" =>> ({}, {})", x, y);
    return {x, y};
  };

  if (!fs::exists(_cache)) {
    logger::info("Creating tile cache directory \"{}\"", _cache.c_str());
    if (!fs::create_directory(_cache)) {
      logger::warning("Failed to create cache directory!!!");
    }
  }
  
  std::vector<tile_data> tiles;
  tiles.reserve(tile_count);
  for (int32 tile_x = min_tile.x; tile_x <= max_tile.x; ++tile_x) {
    for (int32 tile_y = min_tile.y; tile_y <= max_tile.y; ++tile_y) {
      const dvec2 coord = tile2coord({tile_x, tile_y}, zoom);
      const auto filename = fmt::format("osm-{}_{}_{}.png", zoom, tile_x, tile_y);
      const fs::path file = _cache / filename;
      const bool exists = fs::exists(file);
      logger::debug(" - ({}, {}) -> \"{}\" [{}]",
                    tile_x, tile_y, file.c_str(),
                    exists ? "IN CACHE" : "NOT IN CACHE");
      if (!exists) {
        const auto url = format_osm_url({tile_x, tile_y}, zoom);
        if (!download_to_file(url, file.c_str())) {
          logger::error("Failed to download from url \"{}\"", url);
          continue;
        }
      }
      tiles.emplace_back(ntf::load_image<ntf::uint8>(file.string()).value(), coord2pos(coord));
    }
  }
  return tiles;
}

auto osm_map::query_gps() -> gps_query {
  gps_query query;
  query.info = fmt::format("conn: {}\npos: ({}, {})\nsat: {}\nlast update: {}",
                           _gps.available, _gps.lat, _gps.lng, _gps.sat_c, _gps.last_update);
  return query;
}

  //
  // shader_loader loader;
  // auto vert = ntf::file_contents("res/shader/framebuffer.vs.glsl");
  // auto frag = ntf::file_contents("res/shader/framebuffer.fs.glsl");
  // _fbo_shader = loader(vert, frag);
  //
  // const auto min_tile = coord2tile(coord{_box_min.x, _box_min.y}, _zoom);
  // const auto max_tile = coord2tile(coord{_box_max.x, _box_max.y}, _zoom);
  //
  // const std::size_t tile_size = 256;
  // const ntf::ivec2 pixels {
  //   (max_tile.x - min_tile.x + 1)*tile_size,
  //   (max_tile.y - min_tile.y * 1)*tile_size
  // };
  //
  // const std::size_t total = (1 + max_tile.x- min_tile.x)*(1 + max_tile.y - min_tile.y);
  // ntf::log::debug("Fetching {} tiles!", total);
  //
  // for (int i = min_tile.x; i <= max_tile.x; ++i) {
  //   for (int j = min_tile.y; j <= max_tile.y; ++j) {
  //     const ntf::ivec2 tile_coord = {i, j};
  //     const std::string filename = fmt::format("osm-{}_{}_{}.png", _zoom, tile_coord.x, tile_coord.y);
  //     const fs::path file = _cache / filename;
  //     ntf::log::debug("{}", file.c_str());
  //     auto load_image = [&, this](fs::path path) -> ntf::gl_renderer::texture2d {
  //       ntf::texture_data<ntf::gl_renderer::texture2d>::loader loader;
  //       auto filter = ntf::tex_filter::nearest;
  //       auto wrap = ntf::tex_wrap::clamp_edge;
  //
  //       if (fs::exists(path)) {
  //         return loader(path.c_str(), filter, wrap);
  //       }
  //
  //       const std::string url = fmt::format("https://tile.openstreetmap.org/{}/{}/{}.png",
  //         _zoom, tile_coord.x, tile_coord.y
  //       );
  //
  //       ntf::log::debug("{}", url);
  //       download_thing(url, path.c_str());
  //       return loader(path.c_str(), filter, wrap);
  //       return {};
  //     };
  //
  //     _tiles.emplace_back(load_image(file), tile_coord);
  //   }
  // }
  // _sz = (max_tile-min_tile);
  // _sz *= 256;
  // _min_tile = min_tile;
  //
  // auto tsz = std::make_pair(_tiles.front().offset, _tiles.back().offset);
  // _min_pos = osm::tile2coord(
  //   static_cast<ntf::vec2>(tsz.first)+ntf::vec2{0.5f}, _zoom
  // );
  // _max_pos = osm::tile2coord(
  //   static_cast<ntf::vec2>(tsz.second)+ntf::vec2{0.5f}, _zoom
  // );
  //
  // _cam = ntf::camera2d{}
  //   .viewport(size())
  //   .znear(-10.f)
  //   .zfar(1.f)
  //   .pos(static_cast<ntf::vec2>(size())*.5f);
  // _fbo = gl::framebuffer{size()};
  // _fbo.tex().set_filter(ntf::tex_filter::nearest);
  // _fbo.tex().set_wrap(ntf::tex_wrap::repeat);
  //
  // _transform = ntf::transform2d{}.scale(size());
  // return true;
// map::map(fs::path cache_path, coord box_min, coord box_max, std::size_t zoom) :
//   _cache(cache_path),
//   _box_min(box_min), _box_max(box_max), _zoom(zoom) { prepare_tiles(); }
//
// bool map::prepare_tiles() {

// }
//
// ntf::vec2 map::coord2pos(float lat, float lng) {
//   auto sz = size();
//   return ntf::vec2 {
//     sz.y*(lng-_min_pos.y)/(_max_pos.y - _min_pos.y),
//     sz.x*(lat-_min_pos.x)/(_max_pos.x - _min_pos.x),
//   };
// }
//
// osm::map::map_object* map::add_object(gl::texture2d* tex, ntf::vec2 map_coords) {
//   _objects.emplace_back(tex, map_coords, ntf::transform2d{}.pos(coord2pos(
//     map_coords.x, map_coords.y
//   )).scale(tex->dim()));
//   return &_objects.back();
// }
//
// void map::update_object(map_object* obj, ntf::vec2 coords) {
//   obj->map_coords =coords;
//   obj->transform.pos(coord2pos(coords.x, coords.y));
// }
//
// bool download_thing(std::string_view url, std::string_view path) {

//   return false;
// }
//
// bool download_string(std::string_view url, std::string& contents) {

//   return false;
// }
