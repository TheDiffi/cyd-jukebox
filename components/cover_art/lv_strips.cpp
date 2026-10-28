#include "lv_strips.h"

#include <cstring>

namespace esphome {
namespace cover_art {

void write_palette_332(uint8_t *strip) {
  auto *palette = reinterpret_cast<lv_color32_t *>(strip);
  for (int i = 0; i != 256; i++) {
    lv_color32_t c;
    c.ch.red = ((i >> 5) & 7) * 255 / 7;
    c.ch.green = ((i >> 2) & 7) * 255 / 7;
    c.ch.blue = (i & 3) * 255 / 3;
    c.ch.alpha = 0xFF;
    palette[i] = c;
  }
}

void attach_strips(lv_obj_t *parent, const StripLayout &layout, uint8_t *const *bufs, lv_img_dsc_t *dscs) {
  lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(parent, LV_OBJ_FLAG_CLICKABLE);
  for (uint8_t i = 0; i != layout.count; i++) {
    lv_img_dsc_t &dsc = dscs[i];
    memset(&dsc, 0, sizeof(dsc));
    dsc.header.always_zero = 0;
    dsc.header.cf = layout.indexed ? LV_IMG_CF_INDEXED_8BIT : LV_IMG_CF_TRUE_COLOR;
    dsc.header.w = layout.width;
    dsc.header.h = layout.rows;
    dsc.data_size = layout.strip_bytes();
    dsc.data = bufs[i];

    lv_obj_t *img = lv_img_create(parent);
    lv_img_set_src(img, &dsc);
    lv_obj_set_pos(img, 0, i * layout.rows * layout.zoom);
    if (layout.zoom != 1) {
      lv_img_set_pivot(img, 0, 0);
      lv_img_set_zoom(img, LV_IMG_ZOOM_NONE * layout.zoom);
      lv_img_set_antialias(img, false);
    }
  }
}

void invalidate_strips(lv_obj_t *parent) { lv_obj_invalidate(parent); }

}  // namespace cover_art
}  // namespace esphome
