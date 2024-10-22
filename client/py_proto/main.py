import pygame
import time

import hashlib
import math
import os
from urllib.request import urlretrieve

Inf = float("inf")

def interpolate(p1, p2, t):
    return (
        p1[0]*t + p2[0]*(1-t),
        p1[1]*t + p2[1]*(1-t),
    )

class OSMManager:
    def __init__(self, **kwargs) -> None:
        cache = kwargs.get("cache")

        self.cache = None

        if cache:
            if not os.path.isdir(cache):
                try:
                    os.makedirs(cache, 0o766)
                    self.cache = cache
                    print("WARNING: Created cache dir", cache)
                except Exception:
                    print("Could not make cache dir", cache)
            elif not os.access(cache, os.R_OK | os.W_OK):
                print("Insufficient privileges on cache dir", cache)
            else:
                self.cache = cache

        if not self.cache:
            self.cache = (
                os.getenv("TMPDIR") or os.getenv("TMP") or os.getenv("TEMP") or "/tmp"
            )
            print(f"WARNING: Using {self.cache} to cache map tiles.")
            if not os.access(self.cache, os.R_OK | os.W_OK):
                print(f" ERROR: Insufficient access to {self.cache}.")
                msg = "Unable to find/create/use map tile cache directory."
                raise Exception(msg)


        self.url = "https://tile.openstreetmap.org/{z}/{x}/{y}.png"

        # Make a hash of the server URL to use in cached tile filenames.
        md5 = hashlib.md5()
        md5.update(self.url.encode("utf-8"))
        self.cache_prefix = f"osmviz-{md5.hexdigest()[:5]}-"


    # Returns (x, y) coords for tile in (lat, lon) in degrees with zoom
    # https://wiki.openstreetmap.org/wiki/Slippy_map_tilenames#Python
    def get_tile_coord(self, lon, lat, zoom):
        lat_rad = lat * math.pi / 180.0
        n = 2.0**zoom
        xtile = int((lon + 180.0) / 360.0 * n)
        ytile = int(
            (1.0 - math.log(math.tan(lat_rad) + (1 / math.cos(lat_rad))) / math.pi)
            / 2.0
            * n
        )
        return xtile, ytile

    # Returns (lat, lon) from the upper left corner of at tile, with (x, y) coords and zoom
    def tile_nw_lat_lon(self, x_tile, y_tile, zoom):
        n = 2.0**zoom
        lon = x_tile / n * 360.0 - 180.0
        lat_rad = math.atan(math.sinh(math.pi * (1 - 2 * y_tile / n)))
        lat = lat_rad * 180.0 / math.pi
        return lat, lon


    # Get tile image from cache, download it if it's not available
    def retrieve_tile_image(self, tile_coord, zoom):
        filename = os.path.join(
            self.cache, f"{self.cache_prefix}{zoom}_{tile_coord[0]}_{tile_coord[1]}.png"
        )
        if not os.path.isfile(filename):
            url = self.url.format(x=tile_coord[0], y=tile_coord[1], z=zoom)
            try:
                urlretrieve(url, filename=filename)
            except Exception as e:
                msg = f"Unable to retrieve URL: {url}\n{e}"
                raise Exception(msg)
        return filename


    # Creates an image from OSM tiles
    def create_osm_image(self, bounds, zoom):
        (min_lat, max_lat, min_lon, max_lon) = bounds

        topleft = min_x, min_y = self.get_tile_coord(min_lon, max_lat, zoom)
        max_x, max_y = self.get_tile_coord(max_lon, min_lat, zoom)
        new_max_lat, new_min_lon = self.tile_nw_lat_lon(min_x, min_y, zoom)
        new_min_lat, new_max_lon = self.tile_nw_lat_lon(max_x + 1, max_y + 1, zoom)
        print(max_x, max_y, min_x, min_y)

        tile_size = 256
        pix_width = (max_x - min_x + 1) * tile_size
        pix_height = (max_y - min_y + 1) * tile_size

        image = pygame.Surface((pix_width, pix_height))
        total = (1 + max_x - min_x) * (1 + max_y - min_y)


        print(f"Fetching {total} tiles...")

        for x in range(min_x, max_x + 1):
            for y in range(min_y, max_y + 1):
                f_name = self.retrieve_tile_image((x, y), zoom)
                x_off = tile_size * (x - min_x)
                y_off = tile_size * (y - min_y)

                img_file = pygame.image.load(f_name)
                image.blit(img_file, (x_off, y_off))
                del img_file

        else:
            print("... done.")
        return (image, (new_min_lat, new_max_lat, new_min_lon, new_max_lon))



class MapObject():
    def __init__(self, img_path, get_coord, get_pos):
        self.img = pygame.image.load(img_path)
        self.w = self.img.get_rect().width
        self.h = self.img.get_rect().height
        self.get_coord = get_coord
        self.get_pos = get_pos

    def tick(self, t):
        ll = self.get_pos(t)
        self.x, self.y = self.get_coord(*ll) 

    def draw(self, surface):
        x, y = self.x - self.w / 2, self.y - self.h / 2
        surface.blit(self.img, (x, y))



bounding_box = (-24.87822, -24.87160, -65.46543, -65.45390)
refresh_rate = 0.01
window_size = (1280, 800)
pygame.init()

osm = OSMManager(cache="maptiles/")
bg_big, new_bounds = osm.create_osm_image(bounding_box, zoom=17)

wh_ratio = float(bg_big.get_width()) / bg_big.get_height()
new_width = int(window_size[1] * wh_ratio)
new_height = int(window_size[0] / wh_ratio)
if new_width > window_size[0]:
    window_size = window_size[0], new_height
elif new_height > window_size[1]:
    window_size = new_width, window_size[1]

screen = pygame.display.set_mode(window_size)

bg_small = pygame.transform.smoothscale(bg_big, window_size)
del bg_big

t = 0
last_t = t

def get_xy(lat, lon):
    x_ratio = (lon - new_bounds[2]) / (new_bounds[3] - new_bounds[2])
    y_ratio = 1.0 - ((lat - new_bounds[0]) / (new_bounds[1] - new_bounds[0]))
    x, y = int(x_ratio * window_size[0]), int(y_ratio * window_size[1])
    return x, y

def cino_pos(t):
    p1 = (-24.872878, -65.462669)
    p2 = (-24.875672, -65.456650)

    return interpolate(p1, p2, t/20)

baka = MapObject("../res/cirno.png", get_xy, cino_pos)

should_close = False

while not should_close:
    last_t = t
    for event in pygame.event.get():
        if event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE:
            should_close = True

    screen.blit(bg_small, (0, 0))

    baka.tick(t)
    baka.draw(screen)
    
    pygame.display.flip()
    time.sleep(refresh_rate)
    t = t + refresh_rate
del bg_small
pygame.display.quit()
