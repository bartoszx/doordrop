import mysql.connector
import imaplib
import email
import re
import asyncio
from datetime import timedelta
import logging
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity import Entity
from homeassistant.helpers.update_coordinator import DataUpdateCoordinator, UpdateFailed
from homeassistant.components.mqtt import async_publish, async_subscribe
from .const import (
    CONF_IMAP_HOST, CONF_IMAP_PORT, CONF_IMAP_USERNAME, CONF_IMAP_PASSWORD,
    CONF_DB_HOST, CONF_DB_PORT, CONF_DB_USERNAME, CONF_DB_PASSWORD, CONF_DB_NAME,
    CONF_SCAN_INTERVAL, CONF_MQTT_TOPIC, CONF_MQTT_STATUS_TOPIC, CONF_BTN_ENTITY_ID
)
from .patterns import PATTERNS
from .search_patterns import find_tracking_code

_LOGGER = logging.getLogger(__name__)

async def async_setup_entry(hass: HomeAssistant, config_entry, async_add_entities):
    """Set up the sensor platform from a config entry."""
    config = config_entry.data

    imap_username = config[CONF_IMAP_USERNAME]
    imap_password = config[CONF_IMAP_PASSWORD]
    imap_host = config[CONF_IMAP_HOST]
    imap_port = config[CONF_IMAP_PORT]
    db_host = config[CONF_DB_HOST]
    db_port = config[CONF_DB_PORT]
    db_username = config[CONF_DB_USERNAME]
    db_password = config[CONF_DB_PASSWORD]
    db_name = config[CONF_DB_NAME]
    scan_interval = config[CONF_SCAN_INTERVAL]
    mqtt_topic = config[CONF_MQTT_TOPIC]
    mqtt_status_topic = config[CONF_MQTT_STATUS_TOPIC]
    button_entity_id = config[CONF_BTN_ENTITY_ID]

    sensor = ShipmentTrackerSensor(
        hass, imap_username, imap_password, imap_host, imap_port,
        db_host, db_port, db_username, db_password, db_name,
        scan_interval, mqtt_topic, mqtt_status_topic, button_entity_id
    )

    async_add_entities([sensor])

    return True

class ShipmentTrackerSensor(Entity):
    def __init__(self, hass, imap_username, imap_password, imap_host, imap_port, db_host, db_port, db_username, db_password, db_name, scan_interval, mqtt_topic, mqtt_status_topic, button_entity_id):
        self.hass = hass
        self._state = None
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
            name="doordrop",
            update_method=self._async_update,
            update_interval=timedelta(minutes=scan_interval)
        )
        self._patterns = PATTERNS

    async def async_added_to_hass(self):
        await self._coordinator.async_config_entry_first_refresh()
        self._subscription = await async_subscribe(self.hass, self._mqtt_topic, self.on_message)
        _LOGGER.debug("Subscribed to MQTT topic: %s", self._mqtt_topic)

    async def on_message(self, message):
        """Handle incoming MQTT messages."""
        try:
            payload = message.payload.decode() if isinstance(message.payload, bytes) else message.payload
            _LOGGER.info("Received message on %s: %s", message.topic, payload)
            await self.process_code(payload)
        except Exception as e:
            _LOGGER.error("Error processing MQTT message: %s", str(e))

    async def _run_db_task(self, code):
        try:
            await self.hass.async_add_executor_job(self.__run_db_task_blocking, code)
        except Exception as e:
            _LOGGER.error("Database operation failed: %s", str(e))

    async def _async_update(self):
        """A wrapper to run the synchronous update method in the executor."""
        await self.hass.async_add_executor_job(self.update)

    def add_to_database(self, code):
        """Add the scanned code to the database synchronously."""
        self.__run_db_task_blocking(code)

    def update(self):
        """Update the sensor by checking for new emails and processing them."""
        try:
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
                        if body:  # Ensure body is not None
                            providers = self.identify_providers(body)
                            extracted_code = None
                            for provider in providers:
                                extracted_code = find_tracking_code(provider, body, self._patterns)
                                if extracted_code:
                                    break
                            if extracted_code:
                                asyncio.run_coroutine_threadsafe(self._run_db_task(extracted_code), self.hass.loop)
            server.close()
            server.logout()
        except Exception as e:
            _LOGGER.error("Error in email fetching or processing: %s", str(e))

    async def process_code(self, code):
        """Process the scanned code and update the system state."""
        _LOGGER.debug("Processing code %s", code)
        if await self.is_code_in_database(code):
            await self.update_code_status(code, active=False)
            _LOGGER.debug("Pressing button with entity_id: %s", self._button_entity_id)
            await self.hass.services.async_call('input_button', 'press', {'entity_id': "input_button." + self._button_entity_id})
            _LOGGER.debug("Publishing Authorized to MQTT")
            await self.publish_status("Authorized")
        else:
            await self.publish_status("Unauthorized")
            _LOGGER.debug("Publishing Unauthorized to MQTT")

    def _get_email_body(self, msg):
        """Extract the body of the email."""
        if msg.is_multipart():
            for part in msg.walk():
                if part.get_content_type() == 'text/plain':
                    payload = part.get_payload(decode=True)
                    return payload.decode() if payload else None
        else:
            payload = msg.get_payload(decode=True)
            return payload.decode() if payload else None
        return None

    def identify_providers(self, email_body):
        """Identify the providers based on the email content."""
        providers = [provider for provider in self._patterns.keys() if provider.lower() in email_body.lower()]
        if not providers:
            _LOGGER.warning("No providers found in the email content")
        return providers

    def __run_db_task_blocking(self, code):
        """Function to run the DB task that potentially blocks, ensuring proper handling of database operations."""
        conn = None
        cursor = None
        try:
            conn = mysql.connector.connect(
                host=self._db_host,
                port=self._db_port,
                user=self._db_username,
                password=self._db_password,
                database=self._db_name
            )
            cursor = conn.cursor()
            cursor.execute("""
                CREATE TABLE IF NOT EXISTS shipments (
                    id INT AUTO_INCREMENT PRIMARY KEY,
                    code VARCHAR(255) UNIQUE,
                    date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                    active BOOLEAN DEFAULT TRUE,
                    checked_date TIMESTAMP NULL
                );
            """)
            cursor.execute("INSERT INTO shipments (code) VALUES (%s) ON DUPLICATE KEY UPDATE code=VALUES(code)", (code,))
            conn.commit()
            _LOGGER.info(f"Added or updated code in database: {code}")
        except Exception as e:
            _LOGGER.error("Error in database operation: %s", str(e))
        finally:
            if cursor is not None:
                cursor.close()
            if conn is not None and conn.is_connected():
                conn.close()

    async def is_code_in_database(self, code):
        """Check if the code exists in the database and is active asynchronously."""
        return await self.hass.async_add_executor_job(self._is_code_in_database_blocking, code)

    def _is_code_in_database_blocking(self, code):
        """Check if the code exists in the database and is active (synchronous version)."""
        conn = None
        cursor = None
        try:
            conn = mysql.connector.connect(
                host=self._db_host,
                port=self._db_port,
                user=self._db_username,
                password=self._db_password,
                database=self._db_name
            )
            cursor = conn.cursor()
            _LOGGER.debug("Checking if code %s is in the database and active.", code)
            cursor.execute("SELECT active FROM shipments WHERE code = %s", (code,))
            result = cursor.fetchone()
            if result is None:
                _LOGGER.info("Code %s not found in the database.", code)
                return False
            return result[0]  # This directly returns True if active, False otherwise
        except Exception as e:
            _LOGGER.error("Error checking code in the database: %s", str(e))
            return False
        finally:
            if cursor is not None:
                cursor.close()
            if conn is not None and conn.is_connected():
                conn.close()

    async def update_code_status(self, code, active):
        """Update the active status of a shipment code in the database asynchronously."""
        await self.hass.async_add_executor_job(self._update_code_status, code, active)

    def _update_code_status(self, code, active):
        """Update the active status of a shipment code in the database synchronously."""
        conn = None
        cursor = None
        try:
            conn = mysql.connector.connect(
                host=self._db_host,
                port=self._db_port,
                user=self._db_username,
                password=self._db_password,
                database=self._db_name
            )
            cursor = conn.cursor()
            cursor.execute("UPDATE shipments SET active = %s WHERE code = %s", (active, code))
            conn.commit()
        except Exception as e:
            _LOGGER.error("Error updating code status in the database: %s", str(e))
            if conn:
                conn.rollback()  # Important to rollback in case of failure
        finally:
            if cursor is not None:
                cursor.close()
            if conn is not None and conn.is_connected():
                conn.close()

    async def publish_status(self, status):
        """Publish the status to the MQTT topic using Home Assistant's MQTT."""
        await async_publish(self.hass, self._mqtt_status_topic, status)
