#ifdef USE_ESP32

#include "cover_art.h"

#include <algorithm>
#include <cstring>

#include <esp_heap_caps.h>
#include <esp_http_client.h>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cover_art {

static const char *const TAG = "cover_art";

static constexpr uint16_t STRIP_ROWS = 15;
// Palette strips carry a 1 KB palette each, so make them taller (but keep each under 16 KB)
static constexpr uint16_t MAX_PALETTE_STRIP_ROWS = 64;
// Byte-addressable heap that must stay free after allocating the strips at boot. WiFi,
// the API and LVGL take ~72 KB of it after this component's setup runs, and a fetch
// needs ~12 KB on top, so this leaves ~24 KB of slack at runtime.
static constexpr size_t MIN_FREE_AFTER_ALLOC = 96 * 1024;
static constexpr uint32_t TASK_STACK = 5120;
static constexpr int HTTP_TIMEOUT_MS = 8000;

// 4x4 ordered dither thresholds
static constexpr uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
// Bilinear weights (x256) for shrinking 16 pixels to 15: centre of output i lies at
// source i + (2i+1)/30, interpolated between source i and i+1
static constexpr uint8_t W16_15[15] = {9, 26, 43, 60, 77, 94, 111, 128, 145, 162, 179, 196, 213, 230, 247};

namespace {

class HttpSource : public JpegSource {
 public:
  HttpSource(esp_http_client_handle_t client, const std::atomic<uint32_t> &request_seq, uint32_t seq)
      : client_(client), request_seq_(request_seq), seq_(seq) {}

  size_t read(uint8_t *buf, size_t len) override {
    // A newer request is waiting: starve the decoder so it gives up
    if (this->request_seq_.load(std::memory_order_relaxed) != this->seq_)
      return 0;
    uint8_t scratch[64];
    size_t done = 0;
    while (done < len) {
      char *dst = reinterpret_cast<char *>(buf != nullptr ? buf + done : scratch);
      size_t want = buf != nullptr ? len - done : std::min(len - done, sizeof(scratch));
      int got = esp_http_client_read(this->client_, dst, want);
      if (got <= 0)
        break;
      done += got;
    }
    this->bytes += done;
    return done;
  }

  size_t bytes{0};

 protected:
  esp_http_client_handle_t client_;
  const std::atomic<uint32_t> &request_seq_;
  uint32_t seq_;
};

inline int clamp_u8(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

}  // namespace

// ── Setup ───────────────────────────────────────────────────────────────────

void CoverArt::setup() {
  this->container_ = this->container_getter_();
  if (this->container_ == nullptr) {
    this->status_set_error(LOG_STR("No LVGL container"));
    this->mark_failed();
    return;
  }

  // Only the visible rows get strips; anything the decoder produces below them is dropped
  const uint16_t height = this->height_;
  uint16_t palette_rows = MAX_PALETTE_STRIP_ROWS;
  while (height % palette_rows != 0)
    palette_rows--;
  const uint16_t half_width = this->size_ / 2;
  const uint16_t half_height = (height + 1) / 2 / STRIP_ROWS * STRIP_ROWS;
  const StripLayout rgb565{this->size_, STRIP_ROWS, static_cast<uint8_t>(height / STRIP_ROWS), false, 1};
  const StripLayout palette{this->size_, palette_rows, static_cast<uint8_t>(height / palette_rows), true, 1};
  const StripLayout halved{half_width, STRIP_ROWS, static_cast<uint8_t>(half_height / STRIP_ROWS), false, 2};

  bool ok = false;
  switch (this->mode_) {
    case Mode::AUTO:
      ok = this->allocate_(rgb565) || this->allocate_(palette) || this->allocate_(halved);
      break;
    case Mode::RGB565:
      ok = this->allocate_(rgb565);
      break;
    case Mode::PALETTE:
      ok = this->allocate_(palette);
      break;
    case Mode::HALF:
      ok = this->allocate_(halved);
      break;
  }
  this->workspace_ = ok ? heap_caps_malloc(ROM_JPEG_WORKSPACE_SIZE, MALLOC_CAP_8BIT) : nullptr;
  if (this->workspace_ == nullptr ||
      xTaskCreatePinnedToCore(task_entry_, "cover_art", TASK_STACK, this, 1, &this->task_, 0) != pdPASS) {
    this->free_strips_();
    this->status_set_error(LOG_STR("Not enough RAM for the cover"));
    this->mark_failed();
    return;
  }

  if (this->layout_.indexed) {
    for (uint8_t i = 0; i != this->layout_.count; i++)
      write_palette_332(this->bufs_[i]);
  }
  this->clear();
  attach_strips(this->container_, this->layout_, this->bufs_, this->dscs_);
}

bool CoverArt::allocate_(const StripLayout &layout) {
  this->layout_ = layout;
  for (uint8_t i = 0; i != layout.count; i++) {
    this->bufs_[i] = static_cast<uint8_t *>(heap_caps_malloc(layout.strip_bytes(), MALLOC_CAP_8BIT));
    if (this->bufs_[i] == nullptr) {
      this->free_strips_();
      return false;
    }
  }
  this->free_after_alloc_ = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  if (this->free_after_alloc_ < MIN_FREE_AFTER_ALLOC) {
    this->free_strips_();
    return false;
  }
  return true;
}

void CoverArt::free_strips_() {
  for (auto &buf : this->bufs_) {
    heap_caps_free(buf);
    buf = nullptr;
  }
  this->layout_ = {};
}

void CoverArt::dump_config() {
  static const char *const MODES[] = {"RGB565", "8-bit palette", "half resolution, 2x zoom"};
  const char *mode = this->layout_.zoom != 1 ? MODES[2] : (this->layout_.indexed ? MODES[1] : MODES[0]);
  ESP_LOGCONFIG(TAG,
                "Cover Art:\n"
                "  URL: %s\n"
                "  Mode: %s\n"
                "  Strips: %u x %ux%u px, %u bytes\n"
                "  Free heap after allocating (before WiFi): %u bytes\n"
                "  Fit: %s, dither: %s",
                this->url_.c_str(), mode, this->layout_.count, this->layout_.width, this->layout_.rows,
                (unsigned) this->layout_.total_bytes(), (unsigned) this->free_after_alloc_,
                this->fit_ == Fit::CROP ? "crop" : "scale", YESNO(this->dither_));
  if (this->is_failed())
    ESP_LOGE(TAG, "  Failed: not enough RAM for the cover strips");
}

// ── Requests ────────────────────────────────────────────────────────────────

void CoverArt::fetch(const std::string &url) {
  if (this->task_ == nullptr)
    return;
  {
    LockGuard guard(this->url_lock_);
    this->pending_url_ = url.empty() ? this->url_ : url;
  }
  this->request_seq_.fetch_add(1);
  xTaskNotifyGive(this->task_);
}

void CoverArt::loop() {
  if (!this->result_ready_.load(std::memory_order_acquire))
    return;
  FetchResult res = this->result_;
  this->result_ready_.store(false, std::memory_order_release);

  bool superseded = res.seq != this->request_seq_.load();
  if (res.decoded)
    invalidate_strips(this->container_);

  if (superseded) {
    ESP_LOGD(TAG, "Cover superseded by a newer request after %u bytes", (unsigned) res.bytes);
    return;
  }
  if (res.error != ESP_OK) {
    ESP_LOGW(TAG, "Cover download failed: %s", esp_err_to_name(res.error));
    return;
  }
  if (res.http_status != 200) {
    ESP_LOGW(TAG, "Cover download failed: HTTP %d", res.http_status);
    return;
  }
  if (res.jpeg != JpegResult::OK && !res.complete) {
    ESP_LOGW(TAG, "Cover decode failed after %u bytes: %s", (unsigned) res.bytes, jpeg_result_str(res.jpeg));
    return;
  }
  ESP_LOGI(TAG, "Cover %ux%u (1/%u) -> %u px %s: %u bytes, headers %u ms, total %u ms", res.src_width,
           res.src_height, 1u << res.scale, res.cells, res.blocks16 ? "(16->15 blocks)" : "(nearest)",
           (unsigned) res.bytes, (unsigned) res.ms_headers, (unsigned) res.ms_total);
  // MALLOC_CAP_8BIT: the debug component's numbers include IRAM, which pixels can't use
  ESP_LOGD(TAG, "Heap free %u (lowest during fetch %u), largest block %u, task stack left %u",
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_8BIT), (unsigned) res.min_free,
           (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), (unsigned) res.stack_left);
}

// ── Fetch task (core 0) ─────────────────────────────────────────────────────

void CoverArt::task_entry_(void *arg) {
  auto *self = static_cast<CoverArt *>(arg);
  uint32_t done_seq = 0;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    // Requests that arrive while one is running bump the sequence; loop until caught up
    uint32_t seq;
    while ((seq = self->request_seq_.load()) != done_seq) {
      std::string url;
      {
        LockGuard guard(self->url_lock_);
        url = self->pending_url_;
      }
      self->run_fetch_(url, seq);
      done_seq = seq;
    }
  }
}

void CoverArt::run_fetch_(const std::string &url, uint32_t seq) {
  // Wait for loop() to pick up the previous result before overwriting it
  while (this->result_ready_.load(std::memory_order_acquire))
    vTaskDelay(pdMS_TO_TICKS(10));

  this->active_seq_ = seq;
  this->working_ = {};
  this->working_.seq = seq;
  const uint32_t start = millis();
  heap_caps_monitor_local_minimum_free_size_start();

  esp_http_client_config_t config{};
  config.url = url.c_str();
  config.timeout_ms = HTTP_TIMEOUT_MS;
  config.buffer_size = 1024;
  config.buffer_size_tx = 1024;
  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == nullptr) {
    this->working_.error = ESP_ERR_NO_MEM;
  } else {
    if (!this->authorization_.empty())
      esp_http_client_set_header(client, "Authorization", this->authorization_.c_str());
    esp_http_client_set_header(client, "Accept", "image/jpeg");
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
      this->working_.error = err;
    } else {
      esp_http_client_fetch_headers(client);
      this->working_.http_status = esp_http_client_get_status_code(client);
      this->working_.ms_headers = millis() - start;
      if (this->working_.http_status == 200) {
        HttpSource source(client, this->request_seq_, seq);
        this->working_.jpeg = rom_jpeg_decode(this->workspace_, ROM_JPEG_WORKSPACE_SIZE, &source, this);
        if (this->working_.jpeg == JpegResult::OK)
          this->working_.complete = true;
        this->working_.bytes = source.bytes;
      }
      esp_http_client_close(client);
    }
    esp_http_client_cleanup(client);
  }
  this->working_.ms_total = millis() - start;
  this->working_.stack_left = uxTaskGetStackHighWaterMark(nullptr);
  this->working_.min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
  heap_caps_monitor_local_minimum_free_size_stop();

  this->result_ = this->working_;
  this->result_ready_.store(true, std::memory_order_release);
}

// ── Decoder callbacks ───────────────────────────────────────────────────────

int CoverArt::on_header(uint16_t width, uint16_t height, uint8_t mcu_width, uint8_t mcu_height) {
  const uint8_t cell = std::min<uint16_t>(this->pixel_size_, this->layout_.width);
  // With a pixel size above 1 the cover is decoded to fewer, larger pixels. TJpgDec's own
  // 1/2..1/8 scaling averages the source, so most of the shrinking is done before we see it.
  const uint16_t target = this->layout_.width / cell;
  // Largest TJpgDec scale-down that keeps the image at least target pixels wide
  int scale = 0;
  while (scale < 3 && (std::min(width, height) >> (scale + 1)) >= target)
    scale++;
  const uint16_t w = width >> scale;
  const uint16_t h = height >> scale;
  uint16_t side = std::min(w, h);
  uint16_t off_x = (w - side) / 2;
  uint16_t off_y = (h - side) / 2;
  if (this->fit_ == Fit::CROP && side > target) {
    // Pixel-perfect: show the centre at 1:1
    off_x += (side - target) / 2;
    off_y += (side - target) / 2;
    side = target;
  }
  // A 256x256 cover of 16x16 blocks shrinks block by block to exactly 240, and each
  // row of blocks lands in exactly one strip
  const bool blocks16 = cell == 1 && w == h && side == target + target / STRIP_ROWS &&
                        (mcu_width >> scale) == 16 && (mcu_height >> scale) == 16;
  const uint8_t origin = (this->layout_.width - target * cell) / 2;
  this->map_ = {off_x, off_y, side, target, cell, origin, blocks16};
  this->working_.src_width = width;
  this->working_.src_height = height;
  this->working_.scale = scale;
  this->working_.blocks16 = blocks16;
  this->working_.cells = target;
  return scale;
}

bool CoverArt::on_block(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *rgb) {
  if (this->request_seq_.load(std::memory_order_relaxed) != this->active_seq_)
    return false;
  // Blocks arrive top to bottom: once they start below the visible rows, stop downloading
  const int first_row = this->map_.blocks16 ? y / 16 * 15
                                            : this->map_.origin + (int(y) - this->map_.off_y) * this->map_.cells /
                                                                      this->map_.side * this->map_.cell;
  if (first_row >= (int) this->layout_.height()) {
    this->working_.complete = true;
    return false;
  }
  this->working_.decoded = true;
  if (this->map_.blocks16 && width == 16 && height == 16) {
    this->block_16_to_15_(x, y, rgb);
  } else {
    this->block_nearest_(x, y, width, height, rgb);
  }
  return true;
}

void CoverArt::block_16_to_15_(uint16_t x, uint16_t y, const uint8_t *rgb) {
  const uint16_t dx = x / 16 * 15;
  const uint16_t dy = y / 16 * 15;
  for (uint8_t j = 0; j != 15; j++) {
    const int wy = W16_15[j];
    const uint8_t *row0 = rgb + j * 16 * 3;
    const uint8_t *row1 = row0 + 16 * 3;
    for (uint8_t i = 0; i != 15; i++) {
      const int wx = W16_15[i];
      const uint8_t *p00 = row0 + i * 3;
      const uint8_t *p10 = row1 + i * 3;
      int c[3];
      for (uint8_t k = 0; k != 3; k++) {
        int top = p00[k] + (((p00[k + 3] - p00[k]) * wx) >> 8);
        int bottom = p10[k] + (((p10[k + 3] - p10[k]) * wx) >> 8);
        c[k] = top + (((bottom - top) * wy) >> 8);
      }
      this->adjust_(c[0], c[1], c[2]);
      this->put_pixel_(dx + i, dy + j, c[0], c[1], c[2]);
    }
  }
}

void CoverArt::block_nearest_(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *rgb) {
  const uint32_t target = this->map_.cells;
  const uint32_t side = this->map_.side;
  for (uint16_t j = 0; j != height; j++) {
    const int sy = y + j - this->map_.off_y;
    if (sy < 0 || sy >= (int) side)
      continue;
    // Each source pixel covers a range of output pixels: empty when shrinking, wider when growing
    const uint16_t dy0 = sy * target / side;
    const uint16_t dy1 = (sy + 1) * target / side;
    for (uint16_t i = 0; i != width; i++) {
      const int sx = x + i - this->map_.off_x;
      if (sx < 0 || sx >= (int) side)
        continue;
      const uint16_t dx0 = sx * target / side;
      const uint16_t dx1 = (sx + 1) * target / side;
      if (dx0 == dx1 || dy0 == dy1)
        continue;
      const uint8_t *p = rgb + (j * width + i) * 3;
      int r = p[0], g = p[1], b = p[2];
      this->adjust_(r, g, b);
      for (uint16_t dy = dy0; dy != dy1; dy++) {
        for (uint16_t dx = dx0; dx != dx1; dx++)
          this->put_cell_(dx, dy, r, g, b);
      }
    }
  }
}

void CoverArt::adjust_(int &r, int &g, int &b) const {
  if (this->saturation_q8_ != 256) {
    const int luma = (77 * r + 150 * g + 29 * b) >> 8;
    r = luma + (((r - luma) * this->saturation_q8_) >> 8);
    g = luma + (((g - luma) * this->saturation_q8_) >> 8);
    b = luma + (((b - luma) * this->saturation_q8_) >> 8);
  }
  if (this->contrast_q8_ != 256) {
    r = 128 + (((r - 128) * this->contrast_q8_) >> 8);
    g = 128 + (((g - 128) * this->contrast_q8_) >> 8);
    b = 128 + (((b - 128) * this->contrast_q8_) >> 8);
  }
}

// One output pixel of the decoded image: a single screen pixel, or a cell x cell block
void CoverArt::put_cell_(uint16_t cx, uint16_t cy, int r, int g, int b) {
  const uint8_t cell = this->map_.cell;
  if (cell == 1) {
    this->put_pixel_(cx, cy, r, g, b);
    return;
  }
  // Dither per block, not per screen pixel, so every block stays one flat colour
  const uint16_t value = this->quantize_(r, g, b, this->dither_ ? BAYER[cy & 3][cx & 3] : 8);
  const uint16_t x0 = this->map_.origin + cx * cell;
  const uint16_t y0 = this->map_.origin + cy * cell;
  for (uint16_t y = y0; y != y0 + cell; y++) {
    for (uint16_t x = x0; x != x0 + cell; x++)
      this->store_(x, y, value);
  }
}

void CoverArt::put_pixel_(uint16_t x, uint16_t y, int r, int g, int b) {
  this->store_(x, y, this->quantize_(r, g, b, this->dither_ ? BAYER[y & 3][x & 3] : 8));
}

// Quantise with an ordered-dither threshold in [0, 1) of a level; 8/16 (no dither) rounds
uint16_t CoverArt::quantize_(int r, int g, int b, uint8_t threshold) const {
  r = clamp_u8(r);
  g = clamp_u8(g);
  b = clamp_u8(b);
  const int t = threshold * 255;
  if (this->layout_.indexed) {
    const uint8_t r3 = (r * 7 * 16 + t) / 4080;
    const uint8_t g3 = (g * 7 * 16 + t) / 4080;
    const uint8_t b2 = (b * 3 * 16 + t) / 4080;
    return (r3 << 5) | (g3 << 2) | b2;
  }
  const uint8_t r5 = (r * 31 * 16 + t) / 4080;
  const uint8_t g6 = (g * 63 * 16 + t) / 4080;
  const uint8_t b5 = (b * 31 * 16 + t) / 4080;
  return pack_rgb565(r5, g6, b5);
}

void CoverArt::store_(uint16_t x, uint16_t y, uint16_t value) {
  const StripLayout &layout = this->layout_;
  if (x >= layout.width || y >= layout.height())
    return;
  const uint8_t strip = y / layout.rows;
  const size_t pos = size_t(y - strip * layout.rows) * layout.width + x;
  if (layout.indexed) {
    this->bufs_[strip][PALETTE_BYTES + pos] = value;
  } else {
    reinterpret_cast<uint16_t *>(this->bufs_[strip])[pos] = value;
  }
}

// ── Test pattern ────────────────────────────────────────────────────────────

void CoverArt::show_test_pattern() {
  if (this->layout_.count == 0)
    return;
  static constexpr uint8_t BARS[8][3] = {{255, 255, 255}, {255, 255, 0}, {0, 255, 255}, {0, 255, 0},
                                         {255, 0, 255},   {255, 0, 0},   {0, 0, 255},   {0, 0, 0}};
  const uint16_t width = this->layout_.width;
  const uint16_t height = this->layout_.height();
  const uint16_t rows = this->layout_.rows;
  for (uint16_t y = 0; y != height; y++) {
    const uint16_t row = y % rows;
    for (uint16_t x = 0; x != width; x++) {
      int r, g, b;
      if (row == 0 || x == 0 || x == width - 1) {
        // White top edge of every strip and white left/right border: seams show as doubled or missing lines
        r = g = b = 255;
      } else if (row == rows - 1) {
        // Red bottom edge of every strip
        r = 255;
        g = b = 0;
      } else if (y < height / 2) {
        const uint8_t *bar = BARS[x * 8 / width];
        r = bar[0];
        g = bar[1];
        b = bar[2];
      } else {
        // Dark grey ramp: banding here means dithering is off
        r = g = b = x * 64 / width;
      }
      this->put_pixel_(x, y, r, g, b);
    }
  }
  invalidate_strips(this->container_);
}

void CoverArt::clear() {
  for (uint8_t i = 0; i != this->layout_.count; i++) {
    uint8_t *pixels = this->bufs_[i] + (this->layout_.indexed ? PALETTE_BYTES : 0);
    memset(pixels, 0, this->layout_.pixel_bytes());
  }
  if (this->container_ != nullptr && this->layout_.count != 0)
    invalidate_strips(this->container_);
}

}  // namespace cover_art
}  // namespace esphome

#endif  // USE_ESP32
