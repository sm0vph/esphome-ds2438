import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_HUMIDITY, CONF_ID, CONF_TEMPERATURE, STATE_CLASS_MEASUREMENT

CONF_MATERIAL = "material"

vtt_mold_index_ns = cg.esphome_ns.namespace("vtt_mold_index")
VTTMoldIndex = vtt_mold_index_ns.class_("VTTMoldIndex", cg.PollingComponent, sensor.Sensor)

MATERIALS = {
    "very_sensitive": 0,
    "sensitive": 1,
    "medium_resistant": 2,
    "resistant": 3,
}

CONFIG_SCHEMA = (
    sensor.sensor_schema(VTTMoldIndex, accuracy_decimals=2, state_class=STATE_CLASS_MEASUREMENT)
    .extend(
        {
            cv.Required(CONF_TEMPERATURE): cv.use_id(sensor.Sensor),
            cv.Required(CONF_HUMIDITY): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_MATERIAL, default="very_sensitive"): cv.enum(MATERIALS, lower=True),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    cg.add(var.set_temperature_sensor(await cg.get_variable(config[CONF_TEMPERATURE])))
    cg.add(var.set_humidity_sensor(await cg.get_variable(config[CONF_HUMIDITY])))
    cg.add(var.set_material(config[CONF_MATERIAL]))
