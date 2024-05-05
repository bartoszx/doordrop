import voluptuous as vol
from homeassistant import config_entries, exceptions
from homeassistant.core import HomeAssistant
import paho.mqtt.client as mqtt
import time
import logging
import imaplib
import mysql.connector
from .const import DOMAIN, CONF_IMAP_HOST, CONF_IMAP_PORT, CONF_IMAP_USERNAME, CONF_IMAP_PASSWORD, CONF_DB_HOST, CONF_DB_PORT, CONF_DB_USERNAME, CONF_DB_PASSWORD, CONF_DB_NAME, CONF_SCAN_INTERVAL, CONF_MQTT_TOPIC, CONF_MQTT_STATUS_TOPIC, CONF_MQTT_USERNAME, CONF_MQTT_PASSWORD, CONF_MQTT_HOST, CONF_MQTT_PORT, CONF_BTN_ENTITY_ID

_LOGGER = logging.getLogger(__name__)

DEFAULT_SCAN_INTERVAL = 1  # 1 minute
DEFAULT_IMAP_PORT = 993
DEFAULT_IMAP_USERNAME = "tracker"
DEFAULT_DB_HOST = "core-mariadb"
DEFAULT_DB_PORT = 3306
DEFAULT_DB_USERNAME = "tracker"
DEFAULT_DB_NAME = "tracker"
DEFAULT_MQTT_TOPIC = "doordrop/kod"
DEFAULT_MQTT_STATUS_TOPIC = "doordrop/status"
DEFAULT_MQTT_USERNAME = "mqtt"
DEFAULT_MQTT_PASSWORD = "mqtt_password"
DEFAULT_MQTT_HOST = "core-mosquitto"
DEFAULT_MQTT_PORT = 1883

DATA_SCHEMA = vol.Schema({
    vol.Required(CONF_IMAP_HOST): str,
    vol.Required(CONF_IMAP_PORT, default=DEFAULT_IMAP_PORT): int,
    vol.Required(CONF_IMAP_USERNAME, default=DEFAULT_IMAP_USERNAME): str,
    vol.Required(CONF_IMAP_PASSWORD): str,
    vol.Required(CONF_DB_HOST, default=DEFAULT_DB_HOST): str,
    vol.Required(CONF_DB_PORT, default=DEFAULT_DB_PORT): int,
    vol.Required(CONF_DB_USERNAME, default=DEFAULT_DB_USERNAME): str,
    vol.Required(CONF_DB_PASSWORD): str,
    vol.Required(CONF_DB_NAME, default=DEFAULT_DB_NAME): str,
    vol.Optional(CONF_SCAN_INTERVAL, default=DEFAULT_SCAN_INTERVAL): vol.All(vol.Coerce(int), vol.Range(min=1)),
    vol.Required(CONF_MQTT_TOPIC, default=DEFAULT_MQTT_TOPIC): str,
    vol.Required(CONF_MQTT_STATUS_TOPIC, default=DEFAULT_MQTT_STATUS_TOPIC): str,
    vol.Required(CONF_MQTT_USERNAME, default=DEFAULT_MQTT_USERNAME): str,
    vol.Required(CONF_MQTT_PASSWORD, default=DEFAULT_MQTT_PASSWORD): str,
    vol.Required(CONF_MQTT_HOST, default=DEFAULT_MQTT_HOST): str,
    vol.Required(CONF_MQTT_PORT, default=DEFAULT_MQTT_PORT): int,    
    vol.Required(CONF_BTN_ENTITY_ID, default='input_button.furtka'): str,  # Dodajemy nowe pole    
})

class ConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    """Handle a config flow for DoorDrop."""

    VERSION = 1
    CONNECTION_CLASS = config_entries.CONN_CLASS_LOCAL_PUSH

    async def async_step_user(self, user_input=None):
        """Handle the initial step."""
        errors = {}
        if user_input is not None:
            try:
                info = await self.validate_input(self.hass, user_input)
                return self.async_create_entry(title=info["title"], data=user_input)
            except CannotConnect as e:
                errors["base"] = str(e)
            except Exception as e:
                errors["base"] = "unknown"
                _LOGGER.exception("Unexpected exception: %s", e)

        return self.async_show_form(
            step_id="user", data_schema=DATA_SCHEMA, errors=errors
        )

    async def validate_input(self, hass: HomeAssistant, data: dict) -> dict[str, str]:
        """Validate user input."""
        # IMAP validation
        try:
            server = imaplib.IMAP4_SSL(data[CONF_IMAP_HOST], data[CONF_IMAP_PORT])
            server.login(data[CONF_IMAP_USERNAME], data[CONF_IMAP_PASSWORD])
            server.logout()
        except Exception as e:
            _LOGGER.error(f"IMAP Connection error: {e}")
            raise CannotConnect("Failed to connect to IMAP server")

        # MySQL validation
        try:
            conn = mysql.connector.connect(
                host=data[CONF_DB_HOST],
                port=data[CONF_DB_PORT],
                user=data[CONF_DB_USERNAME],
                password=data[CONF_DB_PASSWORD],
                database=data[CONF_DB_NAME]
            )
            conn.close()
        except Exception as e:
            _LOGGER.error(f"MySQL Connection error: {e}")
            raise CannotConnect("Failed to connect to MySQL database")

        # MQTT validation
        await hass.async_add_executor_job(self.validate_mqtt, data)

        return {"title": "DoorDrop"}

    def validate_mqtt(self, data):
        """Test connection to the MQTT broker."""
        client = mqtt.Client()
        client.username_pw_set(data[CONF_MQTT_USERNAME], data[CONF_MQTT_PASSWORD])

        connected_flag = False
        def on_connect(client, userdata, flags, rc):
            nonlocal connected_flag
            if rc == 0:
                connected_flag = True
            else:
                _LOGGER.error(f"Failed to connect to MQTT Broker: Connection returned result code {rc}")

        client.on_connect = on_connect
        try:
            client.connect(data[CONF_MQTT_HOST], int(data[CONF_MQTT_PORT]), 60)
            client.loop_start()
            time.sleep(1)  # Wait for connection
            client.loop_stop()
            client.disconnect()
            if not connected_flag:
                raise CannotConnect("Could not connect to MQTT Broker.")
        except Exception as e:
            _LOGGER.error(f"MQTT Connection error: {e}")
            raise CannotConnect("Failed to connect to MQTT Broker")

class CannotConnect(exceptions.HomeAssistantError):
    """Error to indicate we cannot connect."""
