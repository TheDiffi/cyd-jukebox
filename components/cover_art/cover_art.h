#pragma once

#ifdef USE_ESP32

#include <algorithm>
#include <atomic>
#include <functional>
#include <string>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "lv_strips.h"
#include "rom_jpeg.h"

namespace esphome {
namespace cover_art {

enum class Mode : uint8_t { AUTO, RGB565, PALETTE, HALF };
enum class Fit : uint8_t { SCALE, CROP };

// Filled in by the fetch task, read by loop() once `ready` is set
struct FetchResult {
  uint32_t seq{0};
  esp_err_t error{ESP_OK};  // connection error
  int http_status{0};
  JpegResult jpeg{JpegResult::OK};
  bool decoded{false};      // the decoder got as far as writing pixels
  bool complete{false};     // every visible row was written (decoding may stop early)
  size_t min_free{0};       // lowest free heap during this fetch
  size_t bytes{0};
  uint32_t ms_headers{0};
  uint32_t ms_total{0};
  uint16_t src_width{0};
  uint16_t src_height{0};
  uint8_t scale{0};
  bool blocks16{false};
  uint16_t cells{0};        // output resolution (size / pixel size)
  uint32_t stack_left{0};
};

class CoverArt : public Component, protected JpegSink {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  // After LVGL has its draw buffer, before WiFi starts fragmenting the heap
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

  void set_container(std::function<lv_obj_t *()> &&container) { this->container_getter_ = std::move(container); }
  void set_url(const std::string &url) { this->url_ = url; }
  void set_authorization(const std::string &authorization) { this->authorization_ = authorization; }
  void set_size(uint16_t size, uint16_t height) {
    this->size_ = size;
    this->height_ = height;
  }
  void set_mode(Mode mode) { this->mode_ = mode; }
  void set_fit(Fit fit) { this->fit_ = fit; }
  // Takes effect with the next fetch
  void set_pixel_size(uint8_t pixel_size) { this->pixel_size_ = std::max<uint8_t>(pixel_size, 1); }
  uint8_t get_pixel_size() const { return this->pixel_size_; }
  void set_dither(bool dither) { this->dither_ = dither; }
  void set_contrast(float contrast) { this->contrast_q8_ = static_cast<int16_t>(contrast * 256.0f + 0.5f); }
  void set_saturation(float saturation) { this->saturation_q8_ = static_cast<int16_t>(saturation * 256.0f + 0.5f); }

  // Download and show a cover. An empty url uses the configured one. A newer
  // request aborts the one in flight, so rapid skips only decode the last cover.
  void fetch(const std::string &url = "");
  // Colour bars, a grey ramp and a 1px frame around every strip
  void show_test_pattern();
  void clear();

 protected:
  bool allocate_(const StripLayout &layout);
  void free_strips_();

  static void task_entry_(void *arg);
  void run_fetch_(const std::string &url, uint32_t seq);

  int on_header(uint16_t width, uint16_t height, uint8_t mcu_width, uint8_t mcu_height) override;
  bool on_block(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *rgb) override;
  void block_16_to_15_(uint16_t x, uint16_t y, const uint8_t *rgb);
  void block_nearest_(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *rgb);
  void adjust_(int &r, int &g, int &b) const;
  void put_cell_(uint16_t cx, uint16_t cy, int r, int g, int b);
  void put_pixel_(uint16_t x, uint16_t y, int r, int g, int b);
  uint16_t quantize_(int r, int g, int b, uint8_t threshold) const;
  void store_(uint16_t x, uint16_t y, uint16_t value);

  // Configuration
  std::function<lv_obj_t *()> container_getter_;
  std::string url_;
  std::string authorization_;
  uint16_t size_{240};
  uint16_t height_{240};
  Mode mode_{Mode::AUTO};
  Fit fit_{Fit::SCALE};
  uint8_t pixel_size_{1};
  bool dither_{true};
  int16_t contrast_q8_{256};
  int16_t saturation_q8_{256};

  // Strips
  lv_obj_t *container_{nullptr};
  StripLayout layout_{};
  uint8_t *bufs_[MAX_STRIPS]{};
  lv_img_dsc_t dscs_[MAX_STRIPS]{};
  size_t free_after_alloc_{0};

  // Fetch task
  TaskHandle_t task_{nullptr};
  void *workspace_{nullptr};
  Mutex url_lock_;
  std::string pending_url_;
  std::atomic<uint32_t> request_seq_{0};
  uint32_t active_seq_{0};

  // Source-to-strip mapping for the decode in flight (task only)
  struct {
    uint16_t off_x, off_y;  // top-left of the square that is shown, in scaled source pixels
    uint16_t side;          // its size
    uint16_t cells;         // output resolution: the square becomes cells x cells blocks
    uint8_t cell;           // screen pixels per block side (the pixel size)
    uint8_t origin;         // margin that centres cells * cell inside the strips
    bool blocks16;          // fast path: every 16x16 block becomes 15x15
  } map_{};
  FetchResult working_{};

  FetchResult result_{};
  std::atomic<bool> result_ready_{false};
};

template<typename... Ts> class FetchAction : public Action<Ts...>, public Parented<CoverArt> {
 public:
  explicit FetchAction(CoverArt *parent) : Parented<CoverArt>(parent) {}
  TEMPLATABLE_VALUE(std::string, url)

  void play(const Ts &...x) override { this->parent_->fetch(this->url_.has_value() ? this->url_.value(x...) : ""); }
};

template<typename... Ts> class TestPatternAction : public Action<Ts...>, public Parented<CoverArt> {
 public:
  explicit TestPatternAction(CoverArt *parent) : Parented<CoverArt>(parent) {}
  void play(const Ts &...x) override { this->parent_->show_test_pattern(); }
};

template<typename... Ts> class ClearAction : public Action<Ts...>, public Parented<CoverArt> {
 public:
  explicit ClearAction(CoverArt *parent) : Parented<CoverArt>(parent) {}
  void play(const Ts &...x) override { this->parent_->clear(); }
};

}  // namespace cover_art
}  // namespace esphome

#endif  // USE_ESP32
