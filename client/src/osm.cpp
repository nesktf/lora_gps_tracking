#include "osm.hpp"

#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>

#include <nlohmann/json.hpp>

namespace curlopts = curlpp::Options;

static const char* CURL_UA =
  "Mozilla/5.0 (Windows NT 6.1; Win64; x64; rv:47.0) Gecko/20100101 Firefox/47.0";

static std::string format_osm_url(tile_coord tile, uint32 zoom) {
  return fmt::format("https://tile.openstreetmap.org/{}/{}/{}.png",
                     zoom, tile.x, tile.y);
}

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

// https://wiki.openstreetmap.org/wiki/Slippy_map_tilenames#Common_programming_languages
static tile_coord coord2tile(gps_coord coord, uint32 zoom) {
  const auto lat = glm::radians(coord.x);
  const auto n = static_cast<float>(int{1 << zoom});
  const int xtile = static_cast<int>(n*(coord.y + 180.) / 360.);
  const int ytile = static_cast<int>((1.-std::log(std::tan(lat) + (1./std::cos(lat)))/M_PI)*.5*n);
  return {xtile, ytile};
}

static gps_coord tile2coord(tile_coord tile, uint32 zoom) {
  const auto n = static_cast<float>(int{1 << zoom});
  const float lon = (tile.x/n)*360. - 180.;
  const float lat = glm::degrees(std::atan(std::sinh(M_PI*(1-2*tile.y / n))));
  return {lat, lon};
}

osm_tileset::osm_tileset(std::vector<tile_t>&& tiles, vec2 min_coord,
                         vec2 max_coord, vec2 size) noexcept :
  _tiles{std::move(tiles)}, _min_coord{min_coord}, _max_coord{max_coord}, _size{size} {}

vec2 osm_tileset::pos_from_coord(gps_coord coord) const {
  // In world space, lat maps to y and lng to x
  const float fac_x = _size.x/(_max_coord.y-_min_coord.y);
  const float fac_y = _size.y/(_max_coord.x-_min_coord.x);
  return {fac_x*(coord.y-_min_coord.y), fac_y*(coord.x-_min_coord.x)};
}
gps_coord osm_tileset::coord_from_pos(vec2 pos) const {
  const float fac_lat = (_max_coord.x-_min_coord.x)/_size.y;
  const float fac_lng = (_max_coord.y-_min_coord.y)/_size.x;
  return {pos.y*fac_lat + _min_coord.x, pos.x*fac_lng + _min_coord.y};
}

osm_map::osm_map(fs::path cache_path) :
  _cache{cache_path}, _gps{} {}

osm_tileset osm_map::load_tiles(gps_coord min_coord, gps_coord max_coord, uint32 zoom) {
  const auto min_tile = coord2tile(min_coord, zoom);
  const auto max_tile = coord2tile(max_coord, zoom);
  logger::debug("[osm_map] min: ({} {}), max: ({} {})",
                min_tile.x, min_tile.y, max_tile.x, max_tile.y);

  const uint32 tile_count = (1 + max_tile.x - min_tile.x)*(1 + max_tile.y - min_tile.y);
  logger::info("[osm_map] Fetching {} tiles!", tile_count);

  if (!fs::exists(_cache)) {
    logger::info("Creating tile cache directory \"{}\"", _cache.c_str());
    if (!fs::create_directory(_cache)) {
      logger::warning("Failed to create cache directory!!!");
    }
  }
  constexpr float TILE_SIZE = static_cast<float>(osm_tileset::TILE_SIZE);
  const vec2 tileset_sz {
    (max_tile.x - min_tile.x) *  TILE_SIZE,
    (max_tile.y - min_tile.y) * -TILE_SIZE // Negate to match opengl coordinates
  }; //in world space
  logger::debug("TILESET: {}x{}", tileset_sz.x, tileset_sz.y);
  
  std::vector<osm_tileset::tile_t> tiles;
  tiles.reserve(tile_count);

  // The rendering quad is centered at (0,0) instead of (.5, .5)
  constexpr float QUAD_CORRECTION = .5f;
  for (int32 tile_x = min_tile.x; tile_x <= max_tile.x; ++tile_x) {
    for (int32 tile_y = min_tile.y; tile_y <= max_tile.y; ++tile_y) {
      const vec2 world_pos{
        (tile_x-min_tile.x+QUAD_CORRECTION)*TILE_SIZE,
        (tile_y-min_tile.y+QUAD_CORRECTION)*-TILE_SIZE
      };
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
      tiles.emplace_back(ntf::load_image<ntf::uint8>(file.string()).value(), world_pos);
    }
  }
  // Convert the tiles again to clamp set the gps coordinate at the corner of the tile
  return {std::move(tiles), tile2coord(min_tile, zoom), tile2coord(max_tile, zoom), tileset_sz};
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
