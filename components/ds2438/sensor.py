import esphome.codegen as cg
from esphome.components import one_wire, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_HUMIDITY,
    CONF_ID,
    CONF_MODEL,
    CONF_TEMPERATURE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_VOLT,
)

AUTO_LOAD = ["sensor"]
DEPENDENCIES = ["one_wire"]

CONF_HUMIDITY_RAW = "humidity_raw"
CONF_VAD = "vad"
CONF_VDD = "vdd"

ds2438_ns = cg.esphome_ns.namespace("ds2438")
HumidityModel = ds2438_ns.enum("HumidityModel", is_class=True)
DS2438Component = ds2438_ns.class_(
    "DS2438Component", cg.PollingComponent, one_wire.OneWireDevice
)

HUMIDITY_MODELS = {
    "HIH4031": HumidityModel.HIH4031,
}

HUMIDITY_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_PERCENT,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_HUMIDITY,
    state_class=STATE_CLASS_MEASUREMENT,
).extend({cv.Required(CONF_MODEL): cv.enum(HUMIDITY_MODELS, upper=True)})

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DS2438Component),
            cv.Required(CONF_HUMIDITY): HUMIDITY_SCHEMA,
            cv.Optional(CONF_HUMIDITY_RAW): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_VAD): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_VDD): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(one_wire.one_wire_device_schema())
    .extend(cv.polling_component_schema("30s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await one_wire.register_one_wire_device(var, config)

    humidity_config = config[CONF_HUMIDITY]
    humidity = await sensor.new_sensor(humidity_config)
    cg.add(var.set_humidity_sensor(humidity))
    cg.add(var.set_humidity_model(humidity_config[CONF_MODEL]))

    for key, setter in (
        (CONF_HUMIDITY_RAW, var.set_humidity_raw_sensor),
        (CONF_TEMPERATURE, var.set_temperature_sensor),
        (CONF_VAD, var.set_vad_sensor),
        (CONF_VDD, var.set_vdd_sensor),
    ):
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(setter(sens))
