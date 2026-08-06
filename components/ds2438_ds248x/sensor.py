import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
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

CONF_DS2438_ADDRESS = "ds2438_address"
CONF_HUMIDITY_RAW = "humidity_raw"
CONF_VAD = "vad"
CONF_VDD = "vdd"

ds2438_ds248x_ns = cg.esphome_ns.namespace("ds2438_ds248x")
HumidityModel = ds2438_ds248x_ns.enum("HumidityModel", is_class=True)
DS2438DS248xComponent = ds2438_ds248x_ns.class_(
    "DS2438DS248xComponent", cg.PollingComponent, i2c.I2CDevice
)

HUMIDITY_MODELS = {"HIH4031": HumidityModel.HIH4031}

HUMIDITY_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_PERCENT,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_HUMIDITY,
    state_class=STATE_CLASS_MEASUREMENT,
).extend({cv.Required(CONF_MODEL): cv.enum(HUMIDITY_MODELS, upper=True)})

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DS2438DS248xComponent),
            cv.Required(CONF_DS2438_ADDRESS): cv.uint64_t,
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
    .extend(cv.polling_component_schema("never"))
    .extend(i2c.i2c_device_schema(0x18))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_ds2438_address(config[CONF_DS2438_ADDRESS]))
    cg.add(var.set_humidity_model(config[CONF_HUMIDITY][CONF_MODEL]))
    humidity = await sensor.new_sensor(config[CONF_HUMIDITY])
    cg.add(var.set_humidity_sensor(humidity))

    for key, setter in (
        (CONF_HUMIDITY_RAW, "set_humidity_raw_sensor"),
        (CONF_TEMPERATURE, "set_temperature_sensor"),
        (CONF_VAD, "set_vad_sensor"),
        (CONF_VDD, "set_vdd_sensor"),
    ):
        if key in config:
            value = await sensor.new_sensor(config[key])
            cg.add(getattr(var, setter)(value))
