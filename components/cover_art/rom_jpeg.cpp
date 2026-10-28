#ifdef USE_ESP32

#include "rom_jpeg.h"

#include "rom/tjpgd.h"

namespace esphome {
namespace cover_art {

namespace {

struct Session {
  JpegSource *source;
  JpegSink *sink;
};

UINT input_cb(JDEC *jd, BYTE *buf, UINT len) {
  auto *session = static_cast<Session *>(jd->device);
  return session->source->read(buf, len);
}

UINT output_cb(JDEC *jd, void *bitmap, JRECT *rect) {
  auto *session = static_cast<Session *>(jd->device);
  return session->sink->on_block(rect->left, rect->top, rect->right - rect->left + 1, rect->bottom - rect->top + 1,
                                 static_cast<const uint8_t *>(bitmap))
             ? 1
             : 0;
}

JpegResult map_result(JRESULT res) {
  switch (res) {
    case JDR_OK:
      return JpegResult::OK;
    case JDR_INTR:
      return JpegResult::INTERRUPTED;
    case JDR_INP:
      return JpegResult::INPUT;
    case JDR_MEM1:
    case JDR_MEM2:
      return JpegResult::MEMORY;
    case JDR_PAR:
      return JpegResult::PARAMETER;
    case JDR_FMT1:
      return JpegResult::FORMAT;
    default:
      return JpegResult::UNSUPPORTED;
  }
}

}  // namespace

JpegResult rom_jpeg_decode(void *workspace, size_t workspace_size, JpegSource *source, JpegSink *sink) {
  Session session{source, sink};
  JDEC jd;
  JRESULT res = jd_prepare(&jd, input_cb, workspace, workspace_size, &session);
  if (res != JDR_OK)
    return map_result(res);
  int scale = sink->on_header(jd.width, jd.height, jd.msx * 8, jd.msy * 8);
  if (scale < 0)
    return JpegResult::INTERRUPTED;
  return map_result(jd_decomp(&jd, output_cb, static_cast<BYTE>(scale)));
}

const char *jpeg_result_str(JpegResult result) {
  switch (result) {
    case JpegResult::OK:
      return "ok";
    case JpegResult::INTERRUPTED:
      return "interrupted";
    case JpegResult::INPUT:
      return "input error";
    case JpegResult::MEMORY:
      return "out of decoder memory";
    case JpegResult::PARAMETER:
      return "bad parameter";
    case JpegResult::FORMAT:
      return "damaged data";
    default:
      return "unsupported JPEG (progressive?)";
  }
}

}  // namespace cover_art
}  // namespace esphome

#endif  // USE_ESP32
