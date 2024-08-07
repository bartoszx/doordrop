import mysql.connector
import imaplib
import email
import re
import asyncio
from datetime import datetime, timedelta
import logging
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity import Entity
from homeassistant.helpers.update_coordinator import DataUpdateCoordinator, UpdateFailed
from homeassistant.components.mqtt import async_publish, async_subscribe
from .const import (
    CONF_IMAP_HOST, CONF_IMAP_PORT, CONF_IMAP_USERNAME, CONF_IMAP_PASSWORD,
    CONF_DB_HOST, CONF_DB_PORT, CONF_DB_USERNAME, CONF_DB_PASSWORD, CONF_DB_NAME,
    CONF_SCAN_INTERVAL, CONF_MQTT_TOPIC, CONF_MQTT_STATUS_TOPIC, AUTHORIZED_BARCODES
)
from .patterns import PATTERNS, CUSTOM_PATTERNS
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
    authorized_barcodes = config.get(AUTHORIZED_BARCODES, "")

    sensor = ShipmentTrackerSensor(
        hass, "Shipment Tracker", imap_username, imap_password, imap_host, imap_port,
        db_host, db_port, db_username, db_password, db_name,
        scan_interval, mqtt_topic, mqtt_status_topic, authorized_barcodes
    )

    async_add_entities([sensor], True)

    return True

class ShipmentTrackerSensor(Entity):
    def __init__(self, hass, name, imap_username, imap_password, imap_host, imap_port, db_host, db_port, db_username, db_password, db_name, scan_interval, mqtt_topic, mqtt_status_topic, authorized_barcodes):
        self.hass = hass
        self._name = name
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
        self._mqtt_topic = mqtt_topic
        self._mqtt_status_topic = mqtt_status_topic
        self._authorized_barcodes = authorized_barcodes
        self._coordinator = DataUpdateCoordinator(
            hass,
            _LOGGER,
            name="doordrop",
            update_method=self._async_update,
            update_interval=timedelta(minutes=scan_interval)
        )
        self._patterns = PATTERNS

    @property
    def name(self):
        return self._name

    @property
    def state(self):
        return self._state

    def update_status(self, status):
        _LOGGER.debug("Updating status to: %s", status)
        self._state = status
        _LOGGER.debug("Calling async_write_ha_state()")
        self.async_write_ha_state()
        _LOGGER.debug("State updated to: %s", self._state)

    async def async_added_to_hass(self):
        _LOGGER.debug("Adding to hass: %s", self._name)
        await self._coordinator.async_config_entry_first_refresh()
        try:
            self._subscription = await async_subscribe(self.hass, self._mqtt_topic, self.on_message)
            _LOGGER.debug("Subscribed to MQTT topic: %s", self._mqtt_topic)
        except Exception as e:
            _LOGGER.error("Failed to subscribe to MQTT topic: %s", str(e))
            self._subscription = None
        _LOGGER.debug("Subscription set to: %s", self._subscription)




    async def async_will_remove_from_hass(self):
        _LOGGER.debug("Removing from hass: %s", self._name)
        if self._subscription is not None:
            try:
                _LOGGER.debug("Unsubscribing from MQTT topic: %s", self._mqtt_topic)
                await self._subscription()
            except Exception as e:
                _LOGGER.error("Error unsubscribing from MQTT: %s", e)
        else:
            _LOGGER.debug("Subscription is None, nothing to unsubscribe.")
        _LOGGER.debug("Sensor %s removed from hass", self._name)





    async def on_message(self, message):
        """Handle incoming MQTT messages."""
        try:
            _LOGGER.debug("Received MQTT message: %s", message)
            payload = message.payload.decode() if isinstance(message.payload, bytes) else message.payload
            _LOGGER.info("Received message on %s: %s", message.topic, payload)
            await self.process_code(payload)
        except Exception as e:
            _LOGGER.error("Error processing MQTT message: %s", str(e))

    async def process_code(self, code):
        """Process the scanned code and update the system state."""
        _LOGGER.debug("Processing code %s", code)
        is_authorized = await self.is_code_in_database(code) or self.is_authorized_barcode(code)
        if is_authorized:
            await self.update_code_status(code, False)
            self.update_status("authorized")
            _LOGGER.debug("Publishing Authorized to MQTT")
            await self.publish_status("Authorized")
            self.update_status("none")
        else:
            self.update_status("unauthorized")
            _LOGGER.debug("Publishing Unauthorized to MQTT")
            await self.publish_status("Unauthorized")
            self.update_status("none")

    async def _async_update(self):
        """A wrapper to run the synchronous update method in the executor."""
        _LOGGER.debug("Running async update")
        await self.hass.async_add_executor_job(self.update)

    def update(self):
        """Update the sensor by checking for new emails and processing them."""
        _LOGGER.debug("Updating sensor: %s", self._name)
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
                        subject = msg["subject"]
                        body = self._get_email_body(msg)
                        if body:  # Ensure body is not None
                            try:
                                providers = self.identify_providers(subject, body)
                                extracted_code = None
                                for provider in providers:
                                    extracted_code = find_tracking_code(provider, body, self._patterns)
                                    if extracted_code:
                                        break
                                if extracted_code:
                                    asyncio.run_coroutine_threadsafe(self._run_db_task(extracted_code), self.hass.loop)
                            except re.error as e:
                                _LOGGER.error("Error in regex pattern: %s", str(e))
            server.close()
            server.logout()
        except Exception as e:
            _LOGGER.error("Error in email fetching or processing: %s", str(e))

    def _get_email_body(self, msg):
        """Extract the body from the email message."""
        if msg.is_multipart():
            for part in msg.walk():
                content_type = part.get_content_type()
                content_disposition = str(part.get("Content-Disposition"))
                if "attachment" not in content_disposition:
                    if content_type == "text/plain":
                        return part.get_payload(decode=True).decode()
        else:
            return msg.get_payload(decode=True).decode()
        return None

    def identify_providers(self, subject, body):
        """Identify providers based on the subject and body content."""
        providers = []

        _LOGGER.debug("Subject: %s", subject)
        _LOGGER.debug("Body: %s", body)

        for provider, patterns in CUSTOM_PATTERNS.items():
            for pattern in patterns:
                subject_match = re.search(pattern, subject, re.IGNORECASE)
                body_match = re.search(pattern, body, re.IGNORECASE)
                _LOGGER.debug("Testing pattern '%s' for provider '%s'", pattern, provider)
                if subject_match or body_match:
                    _LOGGER.debug("Match found for provider '%s' with pattern '%s'", provider, pattern)
                    providers.append(provider)
                    break
                else:
                    _LOGGER.debug("No match for provider '%s' with pattern '%s'", provider, pattern)

        return providers

    def is_authorized_barcode(self, barcode):
        """Check if the barcode is in the list of authorized barcodes."""
        _LOGGER.debug("Checking if barcode is authorized: %s", barcode)
        return barcode in self._authorized_barcodes.split(",")

    async def is_code_in_database(self, code):
        """Check if the code exists in the database and is active asynchronously."""
        _LOGGER.debug("Checking if code is in database: %s", code)
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

    async def _run_db_task(self, code):
        try:
            _LOGGER.debug("Running DB task for code: %s", code)
            await self.hass.async_add_executor_job(self.__run_db_task_blocking, code)
        except Exception as e:
            _LOGGER.error("Database operation failed: %s", str(e))

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

    async def update_code_status(self, code, active):
        """Update the active status of a shipment code in the database asynchronously."""
        _LOGGER.debug("Updating code status in database: %s, active: %s", code, active)
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
            cursor.execute("UPDATE shipments SET active = %s, checked_date = %s WHERE code = %s", (active, datetime.now(), code))
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
        _LOGGER.debug("Publishing status to MQTT: %s", status)
        await async_publish(self.hass, self._mqtt_status_topic, status)
