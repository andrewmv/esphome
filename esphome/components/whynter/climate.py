import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate_ir, sensor, switch
from esphome.const import (
    CONF_ID,
    CONF_SENSOR,
)

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@andrewmv"]

whynter_ns = cg.esphome_ns.namespace("whynter")
WhynterClimate = whynter_ns.class_(
    "WhynterClimate", climate_ir.ClimateIR
)

CONFIG_SCHEMA = climate_ir.CLIMATE_IR_WITH_RECEIVER_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(WhynterClimate),
        cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await climate_ir.register_climate_ir(var, config)

    if CONF_SENSOR in config:
        sens = await cg.get_variable(config[CONF_SENSOR])
        cg.add(var.set_sensor(sens))
