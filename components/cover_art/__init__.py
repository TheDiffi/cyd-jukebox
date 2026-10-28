"""Full-size album art for LVGL on RAM-starved ESP32s.

Streams a JPEG over HTTP through the ESP32's ROM decoder (TJpgDec) into
small RGB565 strips that LVGL shows as one picture. No large contiguous
buffer, no copy of the JPEG in RAM.
"""

from esphome import automation
import esphome.codegen as cg
from esphome.components import esp32
from esphome.components.esp32.const import VARIANT_ESP32
from esphome.components.lvgl.helpers import add_lv_use
from esphome.components.lvgl.types import lv_obj_t
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODE, CONF_URL

CODEOWNERS = ["@TheDiffi"]
DEPENDENCIES = ["lvgl", "network"]

CONF_AUTHORIZATION = "authorization"
CONF_CONTAINER = "container"
CONF_CONTRAST = "contrast"
CONF_DITHER = "dither"
CONF_FIT = "fit"
CONF_HEIGHT = "height"
CONF_PIXEL_SIZE = "pixel_size"
CONF_SATURATION = "saturation"
CONF_SIZE = "size"

cover_art_ns = cg.esphome_ns.namespace("cover_art")
CoverArt = cover_art_ns.class_("CoverArt", cg.Component)
FetchAction = cover_art_ns.class_("FetchAction", automation.Action)
TestPatternAction = cover_art_ns.class_("TestPatternAction", automation.Action)
ClearAction = cover_art_ns.class_("ClearAction", automation.Action)

Mode = cover_art_ns.enum("Mode", is_class=True)
MODES = {
    # Try RGB565, then palette, then half resolution, whichever fits in RAM
    "AUTO": Mode.AUTO,
    "RGB565": Mode.RGB565,
    "PALETTE": Mode.PALETTE,
    "HALF": Mode.HALF,
}

Fit = cover_art_ns.enum("Fit", is_class=True)
FITS = {
    "SCALE": Fit.SCALE,
    "CROP": Fit.CROP,
}


def _require_esp32(config):
    if esp32.get_esp32_variant() != VARIANT_ESP32:
        raise cv.Invalid(
            "cover_art uses the ESP32's ROM JPEG decoder; only the original ESP32 is supported"
        )
    return config


def _validate_size(config):
    size = config[CONF_SIZE]
    config.setdefault(CONF_HEIGHT, size)
    if size % 15 != 0 or config[CONF_HEIGHT] % 15 != 0:
        raise cv.Invalid("size and height must be multiples of 15 (strips are 15 rows high)")
    if config[CONF_HEIGHT] > size:
        raise cv.Invalid("height can't be larger than size")
    return config


def _use_lvgl_image(config):
    # The images are created from C++, so tell the LVGL build to compile lv_img
    add_lv_use("img")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CoverArt),
            cv.Required(CONF_CONTAINER): cv.use_id(lv_obj_t),
            cv.Required(CONF_URL): cv.url,
            cv.Optional(CONF_AUTHORIZATION, default=""): cv.string,
            cv.Optional(CONF_SIZE, default=240): cv.int_range(min=60, max=240),
            # Rows that are actually visible; RAM is only spent on these
            cv.Optional(CONF_HEIGHT): cv.int_range(min=15, max=240),
            cv.Optional(CONF_MODE, default="AUTO"): cv.enum(MODES, upper=True),
            cv.Optional(CONF_FIT, default="SCALE"): cv.enum(FITS, upper=True),
            # Draw the cover as blocks of N x N screen pixels for a pixel-art look
            cv.Optional(CONF_PIXEL_SIZE, default=1): cv.int_range(min=1, max=16),
            cv.Optional(CONF_DITHER, default=True): cv.boolean,
            cv.Optional(CONF_CONTRAST, default=1.0): cv.float_range(min=0.0, max=3.0),
            cv.Optional(CONF_SATURATION, default=1.0): cv.float_range(min=0.0, max=3.0),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
    _require_esp32,
    _validate_size,
    _use_lvgl_image,
)


async def to_code(config):
    esp32.include_builtin_idf_component("esp_http_client")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # LVGL assigns widget pointers in main setup, so resolve the container lazily
    container = await cg.get_variable(config[CONF_CONTAINER])
    cg.add(var.set_container(cg.RawExpression(f"[]() -> lv_obj_t * {{ return {container}; }}")))
    cg.add(var.set_url(config[CONF_URL]))
    cg.add(var.set_authorization(config[CONF_AUTHORIZATION]))
    cg.add(var.set_size(config[CONF_SIZE], config[CONF_HEIGHT]))
    cg.add(var.set_mode(config[CONF_MODE]))
    cg.add(var.set_fit(config[CONF_FIT]))
    cg.add(var.set_pixel_size(config[CONF_PIXEL_SIZE]))
    cg.add(var.set_dither(config[CONF_DITHER]))
    cg.add(var.set_contrast(config[CONF_CONTRAST]))
    cg.add(var.set_saturation(config[CONF_SATURATION]))


@automation.register_action(
    "cover_art.fetch",
    FetchAction,
    synchronous=True,
    schema=cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(CoverArt),
            # Optional one-off URL, e.g. a Music Assistant imageproxy link
            cv.Optional(CONF_URL): cv.templatable(cv.string),
        },
        key=CONF_ID,
    ),
)
async def fetch_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    if CONF_URL in config:
        templ = await cg.templatable(config[CONF_URL], args, cg.std_string)
        cg.add(var.set_url(templ))
    return var


SIMPLE_ACTION_SCHEMA = cv.maybe_simple_value(
    {cv.GenerateID(): cv.use_id(CoverArt)},
    key=CONF_ID,
)


@automation.register_action(
    "cover_art.test_pattern", TestPatternAction, SIMPLE_ACTION_SCHEMA, synchronous=True
)
@automation.register_action(
    "cover_art.clear", ClearAction, SIMPLE_ACTION_SCHEMA, synchronous=True
)
async def simple_action_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)
