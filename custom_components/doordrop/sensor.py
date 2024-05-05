import mysql.connector
import imaplib
import email
import asyncio
from datetime import datetime, timedelta
import logging
from homeassistant.helpers.entity import Entity
from homeassistant.helpers.update_coordinator import DataUpdateCoordinator, UpdateFailed
from homeassistant.core import HomeAssistant
from homeassistant.const import STATE_OFF, STATE_ON
from .const import DOMAIN, CONF_IMAP_HOST, CONF_IMAP_PORT, CONF_IMAP_USERNAME, CONF_IMAP_PASSWORD, CONF_DB_HOST, CONF_DB_PORT, CONF_DB_USERNAME, CONF_DB_PASSWORD, CONF_DB_NAME, CONF_SCAN_INTERVAL, CONF_MQTT_TOPIC, CONF_MQTT_STATUS_TOPIC, CONF_MQTT_USERNAME, CONF_MQTT_PASSWORD, CONF_MQTT_HOST, CONF_MQTT_PORT, CONF_BTN_ENTITY_ID
import paho.mqtt.client as mqtt
import re
from custom_components.doordrop.patterns import PATTERNS


_LOGGER = logging.getLogger(__name__)

async def async_setup_entry(hass: HomeAssistant, config_entry, async_add_entities):
    config = hass.data[DOMAIN][config_entry.entry_id]
    button_entity_id = config.get(CONF_BTN_ENTITY_ID, 'input_button.furtka')  # Correct constant use

    # Attempt to create the button entity if it does not exist
    # First, ensure that the `input_button.create` service is supported
    if 'input_button.create' in hass.services.async_services():
        # Check if the button entity already exists
        entity_exists = hass.states.get(button_entity_id) is not None
        if not entity_exists:
            await hass.services.async_call('input_button', 'create', {
                'name': button_entity_id.split('.')[1],  # Assumes 'input_button.xyz' format
                'icon': 'mdi:gate'
            }, blocking=True)

    async_add_entities([
        ShipmentTrackerSensor(
            hass, config[CONF_IMAP_USERNAME], config[CONF_IMAP_PASSWORD], config[CONF_IMAP_HOST], config[CONF_IMAP_PORT],
            config[CONF_DB_HOST], config[CONF_DB_PORT], config[CONF_DB_USERNAME], config[CONF_DB_PASSWORD], config[CONF_DB_NAME],
            config[CONF_SCAN_INTERVAL], config[CONF_MQTT_TOPIC], config[CONF_MQTT_STATUS_TOPIC], config[CONF_MQTT_USERNAME],
            config[CONF_MQTT_PASSWORD], config[CONF_MQTT_HOST], config[CONF_MQTT_PORT], button_entity_id
        )
    ])





class ShipmentTrackerSensor(Entity):
    def __init__(self, hass, imap_username, imap_password, imap_host, imap_port, db_host, db_port, db_username, db_password, db_name, scan_interval, mqtt_topic, mqtt_status_topic, mqtt_username, mqtt_password, mqtt_host, mqtt_port, button_entity_id):
        self._hass = hass
        self._state = None
        self._mqtt_subscribed = False        
        self._imap_host = imap_host
        self._imap_port = imap_port
        self._imap_username = imap_username
        self._imap_password = imap_password
        self._db_host = db_host
        self._db_port = db_port
        self._db_username = db_username
        self._db_password = db_password
        self._db_name = db_name
        self._button_entity_id = button_entity_id
        self._mqtt_topic = mqtt_topic
        self._mqtt_status_topic = mqtt_status_topic
        self._coordinator = DataUpdateCoordinator(
            hass,
            _LOGGER,
            name="shipment_tracker",
            update_method=self._update,
            update_interval=timedelta(minutes=scan_interval)
        )
        self._coordinator.async_add_listener(self._update_callback)

        # MQTT client setup
        self.client = mqtt.Client()
        self.client.username_pw_set(mqtt_username, mqtt_password)
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.connect(mqtt_host, mqtt_port, 60)
        self.client.loop_start()
        self.client.subscribe(self._mqtt_topic)

    def on_connect(self, client, userdata, flags, rc):
        """Callback for when the client receives a CONNACK response."""
        if not self._mqtt_subscribed:
            client.subscribe(self._mqtt_topic)
            self._mqtt_subscribed = True
            _LOGGER.info("Subscribed to topic %s", self._mqtt_topic)


    def on_message(self, client, userdata, msg):
        """Callback for when a PUBLISH message is received."""
        _LOGGER.info("Received message on %s: %s", msg.topic, msg.payload.decode())
        code = msg.payload.decode()
        self.process_code(code)

    async def async_added_to_hass(self):
        await self._coordinator.async_refresh()

    def _update_callback(self):
        self.async_schedule_update_ha_state(True)

    async def _update(self):
        try:
            self.update()
        except Exception as err:
            raise UpdateFailed(f"Error updating: {err}")

    def update(self):
        """Update the sensor."""
        server = imaplib.IMAP4_SSL(self._imap_host, self._imap_port)
        server.login(self._imap_username, self._imap_password)
        server.select("inbox")
        status, messages = server.search(None, "UNSEEN")
        for num in messages[0].split():
            status, msg_data = server.fetch(num, "(RFC822)")
            for response_part in msg_data:
                if isinstance(response_part, tuple):
                    msg = email.message_from_bytes(response_part[1])
                    body = self._get_email_body(msg)
                    self._state = self._extract_code(body)
                    if self._state:
                        self.add_to_database(self._state)
        server.close()
        server.logout()

    def _get_email_body(self, msg):
        """Extract the body of the email."""
        if msg.is_multipart():
            for part in msg.walk():
                content_type = part.get_content_type()
                content_disposition = str(part.get("Content-Disposition"))
                if "attachment" not in content_disposition and content_type == "text/plain":
                    return part.get_payload(decode=True).decode()
        else:
            return msg.get_payload(decode=True).decode()

    def _extract_code(self, body):
        """Extract the shipment code using regex from patterns.py."""
        for carrier, pattern in PATTERNS.items():
            match = re.search(pattern, body)
            if match:
                _LOGGER.info(f"Matched {carrier} code: {match.group()}")
                return match.group()
        return None

    def add_to_database(self, code):
        """Add the scanned code to the database."""
        conn = mysql.connector.connect(
            host=self._db_host,
            port=self._db_port,
            user=self._db_username,
            password=self._db_password,
            database=self._db_name,
        )
        _LOGGER.info(f"Trying to insert tracking no and create table if not exist")          
        cursor = conn.cursor()
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS shipments (
                id INT AUTO_INCREMENT PRIMARY KEY,
                code VARCHAR(255) UNIQUE,
                date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                active BOOLEAN DEFAULT TRUE,
                checked_date TIMESTAMP NULL
            )
        """)
        try:
            cursor.execute("INSERT INTO shipments (code) VALUES (%s)", (code,))
            conn.commit()
        except mysql.connector.IntegrityError:
            _LOGGER.warning(f"Shipment code {code} already exists in the database.")
        finally:
            cursor.close()
            conn.close()

    def process_code(self, code):
        """Process the scanned code and update the system state."""
        conn = mysql.connector.connect(
            host=self._db_host,
            port=self._db_port,
            user=self._db_username,
            password=self._db_password,
            database=self._db_name,
        )
        cursor = conn.cursor()
        cursor.execute("SELECT active FROM shipments WHERE code = %s", (code,))
        result = cursor.fetchone()
        cursor.close()
        conn.close()
        
        if result and result[0]:
            # Code found and active
            self.update_code_status(code, active=False)
            self.open_gate()
            self.publish_status("Authorized")
        else:
            # Code not found or inactive
            self.publish_status("Unauthorized")

    def update_code_status(self, code, active):
        """Update the status of the code in the database."""
        _LOGGER.info(f"Trying to update tracking no")        
        conn = mysql.connector.connect(
            host=self._db_host,
            port=self._db_port,
            user=self._db_username,
            password=self._db_password,
            database=self._db_name,
        )
        cursor = conn.cursor()
        cursor.execute("UPDATE shipments SET active = %s, checked_date = %s WHERE code = %s", (active, datetime.now(), code))
        conn.commit()
        cursor.close()
        conn.close()

    def open_gate(self):
        """Simulate opening a gate."""
        self._hass.services.call('input_button', 'press', {'entity_id': self._button_entity_id})

    def publish_status(self, status):
        """Publish the status to the MQTT topic."""
        _LOGGER.info(f"Publishing status: {status}")
        self.client.publish(self._mqtt_status_topic, status)
