import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_HUMIDITY, CONF_ID, CONF_TEMPERATURE, STATE_CLASS_MEASUREMENT, UNIT_PERCENT

CONF_LIMIT_LEVEL_1 = "limit_level_1"
CONF_LIMIT_LEVEL_2 = "limit_level_2"
CONF_LIMIT_LEVEL_3 = "limit_level_3"

mold_risk_index_ns = cg.esphome_ns.namespace("mold_risk_index")
MoldRiskIndex = mold_risk_index_ns.class_("MoldRiskIndex", cg.PollingComponent, sensor.Sensor)

limit_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_PERCENT,
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(MoldRiskIndex, accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT)
    .extend(
        {
            cv.Required(CONF_TEMPERATURE): cv.use_id(sensor.Sensor),
            cv.Required(CONF_HUMIDITY): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_LIMIT_LEVEL_1): limit_schema,
            cv.Optional(CONF_LIMIT_LEVEL_2): limit_schema,
            cv.Optional(CONF_LIMIT_LEVEL_3): limit_schema,
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    cg.add(var.set_temperature_sensor(await cg.get_variable(config[CONF_TEMPERATURE])))
    cg.add(var.set_humidity_sensor(await cg.get_variable(config[CONF_HUMIDITY])))
    for key, setter in (
        (CONF_LIMIT_LEVEL_1, "set_limit_level_1_sensor"),
        (CONF_LIMIT_LEVEL_2, "set_limit_level_2_sensor"),
        (CONF_LIMIT_LEVEL_3, "set_limit_level_3_sensor"),
    ):
        if key in config:
            cg.add(getattr(var, setter)(await sensor.new_sensor(config[key])))
