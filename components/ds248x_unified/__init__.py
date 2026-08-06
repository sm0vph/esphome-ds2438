import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_TEMPERATURE, DEVICE_CLASS_HUMIDITY, DEVICE_CLASS_TEMPERATURE, DEVICE_CLASS_VOLTAGE, STATE_CLASS_MEASUREMENT, UNIT_CELSIUS, UNIT_PERCENT, UNIT_VOLT

CODEOWNERS = []
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor"]
MULTI_CONF = True

CONF_DS18B20 = "ds18b20"
CONF_DS2438 = "ds2438"
CONF_HUMIDITY = "humidity"
CONF_HUMIDITY_RAW = "humidity_raw"
CONF_VAD = "vad"
CONF_VDD = "vdd"
CONF_ACTIVE_PULLUP = "active_pullup"
CONF_STRONG_PULLUP = "strong_pullup"

ns = cg.esphome_ns.namespace("ds248x_unified")
Hub = ns.class_("DS248xUnifiedComponent", cg.PollingComponent, i2c.I2CDevice)
DS18 = ns.class_("DS18B20Sensor", sensor.Sensor)
DS2438 = ns.class_("DS2438Sensor")
temperature_schema = sensor.sensor_schema(unit_of_measurement=UNIT_CELSIUS, accuracy_decimals=1, device_class=DEVICE_CLASS_TEMPERATURE, state_class=STATE_CLASS_MEASUREMENT)
humidity_schema = sensor.sensor_schema(unit_of_measurement=UNIT_PERCENT, accuracy_decimals=1, device_class=DEVICE_CLASS_HUMIDITY, state_class=STATE_CLASS_MEASUREMENT)
voltage_schema = sensor.sensor_schema(unit_of_measurement=UNIT_VOLT, accuracy_decimals=2, device_class=DEVICE_CLASS_VOLTAGE, state_class=STATE_CLASS_MEASUREMENT)
DS18_SCHEMA = sensor.sensor_schema(DS18, unit_of_measurement=UNIT_CELSIUS, accuracy_decimals=1, device_class=DEVICE_CLASS_TEMPERATURE, state_class=STATE_CLASS_MEASUREMENT).extend({cv.Required(CONF_ADDRESS): cv.uint64_t})
DS2438_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(DS2438), cv.Required(CONF_ADDRESS): cv.uint64_t, cv.Required(CONF_HUMIDITY): humidity_schema, cv.Optional(CONF_HUMIDITY_RAW): humidity_schema, cv.Optional(CONF_TEMPERATURE): temperature_schema, cv.Optional(CONF_VAD): voltage_schema, cv.Optional(CONF_VDD): voltage_schema})
CONFIG_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(Hub), cv.Optional(CONF_ACTIVE_PULLUP, default=True): cv.boolean, cv.Optional(CONF_STRONG_PULLUP, default=True): cv.boolean, cv.Required(CONF_DS18B20): cv.ensure_list(DS18_SCHEMA), cv.Required(CONF_DS2438): cv.ensure_list(DS2438_SCHEMA)}).extend(cv.polling_component_schema("60s")).extend(i2c.i2c_device_schema(0x18))

async def to_code(config):
    hub = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(hub, config)
    await i2c.register_i2c_device(hub, config)
    cg.add(hub.set_active_pullup(config[CONF_ACTIVE_PULLUP]))
    cg.add(hub.set_strong_pullup(config[CONF_STRONG_PULLUP]))
    for item in config[CONF_DS18B20]:
        value = await sensor.new_sensor(item)
        cg.add(value.set_address(item[CONF_ADDRESS]))
        cg.add(hub.register_ds18b20(value))
    for item in config[CONF_DS2438]:
        value = cg.new_Pvariable(item[CONF_ID])
        cg.add(value.set_address(item[CONF_ADDRESS]))
        cg.add(value.set_humidity(await sensor.new_sensor(item[CONF_HUMIDITY])))
        if CONF_HUMIDITY_RAW in item: cg.add(value.set_raw(await sensor.new_sensor(item[CONF_HUMIDITY_RAW])))
        if CONF_TEMPERATURE in item: cg.add(value.set_temperature(await sensor.new_sensor(item[CONF_TEMPERATURE])))
        if CONF_VAD in item: cg.add(value.set_vad(await sensor.new_sensor(item[CONF_VAD])))
        if CONF_VDD in item: cg.add(value.set_vdd(await sensor.new_sensor(item[CONF_VDD])))
        cg.add(hub.register_ds2438(value))
