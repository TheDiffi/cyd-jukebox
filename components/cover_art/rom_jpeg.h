#pragma once

// Thin wrapper around the TJpgDec decoder in the ESP32 mask ROM.
// Kept separate because rom/tjpgd.h defines global BYTE/WORD/INT typedefs.

#include <cstddef>
#include <cstdint>

namespace esphome {
namespace cover_art {

// Working memory TJpgDec needs for the Huffman/quantisation tables and one MCU
static constexpr size_t ROM_JPEG_WORKSPACE_SIZE = 3200;

class JpegSource {
 public:
  // Fill buf with exactly len bytes (or skip them if buf is null). Return the number
  // of bytes delivered; anything short of len aborts the decode.
  virtual size_t read(uint8_t *buf, size_t len) = 0;
};

class JpegSink {
 public:
  // Called once the header is parsed. Return the TJpgDec scale (0..3 = 1/1..1/8),
  // or a negative value to abort.
  virtual int on_header(uint16_t width, uint16_t height, uint8_t mcu_width, uint8_t mcu_height) = 0;
  // One decoded block of RGB888 pixels in scaled image coordinates. Return false to abort.
  virtual bool on_block(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *rgb) = 0;
};

enum class JpegResult : uint8_t {
  OK = 0,
  INTERRUPTED,  // sink returned false
  INPUT,        // source ran dry or errored
  MEMORY,
  PARAMETER,
  FORMAT,       // damaged data
  UNSUPPORTED,  // progressive / arithmetic / etc.
};

JpegResult rom_jpeg_decode(void *workspace, size_t workspace_size, JpegSource *source, JpegSink *sink);

const char *jpeg_result_str(JpegResult result);

}  // namespace cover_art
}  // namespace esphome
