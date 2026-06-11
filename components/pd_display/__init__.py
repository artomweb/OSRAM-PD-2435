import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID

DEPENDENCIES = []

PD_display_ns = cg.esphome_ns.namespace("PD_display")
PDDisplayComponent = PD_display_ns.class_("PDDisplayComponent", cg.Component)

CONF_DATA_PINS = "data_pins"
CONF_A0_PIN = "a0_pin"
CONF_A1_PIN = "a1_pin"
CONF_A2_PIN = "a2_pin"
CONF_CE0_PIN = "ce0_pin"
CONF_WR_PIN = "wr_pin"
CONF_RST_PIN = "rst_pin"
CONF_SCROLL_MS = "scroll_ms"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(PDDisplayComponent),
        cv.Required(CONF_DATA_PINS): cv.All(
            cv.ensure_list(pins.gpio_output_pin_schema), cv.Length(min=8, max=8)
        ),
        cv.Required(CONF_A0_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_A1_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_A2_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_CE0_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_WR_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_RST_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_SCROLL_MS, default=300): cv.positive_int,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Process 8 data pins
    data_pins = []
    for pin_config in config[CONF_DATA_PINS]:
        pin = await cg.gpio_pin_expression(pin_config)
        data_pins.append(pin)
    cg.add(var.set_data_pins(data_pins))

    # Process remaining control pins
    for pin_key, setter in [
        (CONF_A0_PIN, var.set_a0_pin),
        (CONF_A1_PIN, var.set_a1_pin),
        (CONF_A2_PIN, var.set_a2_pin),
        (CONF_CE0_PIN, var.set_ce0_pin),
        (CONF_WR_PIN, var.set_wr_pin),
        (CONF_RST_PIN, var.set_rst_pin),
    ]:
        pin = await cg.gpio_pin_expression(config[pin_key])
        cg.add(setter(pin))

    cg.add(var.set_scroll_ms(config[CONF_SCROLL_MS]))
