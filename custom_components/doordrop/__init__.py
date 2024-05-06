from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.const import EVENT_HOMEASSISTANT_START
from homeassistant.components.mqtt import async_subscribe
import logging


_LOGGER = logging.getLogger(__name__)
DOMAIN = "doordrop"

async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry):
    """Setup config entry for Home Assistant."""
    _LOGGER.debug("Setting up DoorDrop component.")

    hass.data.setdefault(DOMAIN, {})
    hass.data[DOMAIN][entry.entry_id] = {
        'config': dict(entry.data),
        'subscription': None
    }


    # Setup MQTT Subscription and check if it was successful
    if not await setup_mqtt_subscriptions(hass, entry):
        _LOGGER.error("Failed to setup MQTT subscriptions.")
        return False

    _LOGGER.debug("MQTT subscription setup complete.")
    if not await hass.config_entries.async_forward_entry_setup(entry, 'sensor'):
        _LOGGER.error("Failed to forward entry setup to sensor platform.")
        return False

    return True

async def setup_mqtt_subscriptions(hass: HomeAssistant, entry: ConfigEntry):
    """Asynchronously set up MQTT subscriptions."""
    try:
        hass.data[DOMAIN][entry.entry_id]['subscription'] = await async_subscribe(
            hass, entry.data['mqtt_topic'], message_callback
        )
        _LOGGER.info("MQTT subscription setup complete.")
        return True
    except Exception as e:
        _LOGGER.error("Failed to set up MQTT subscription: %s", str(e))
        return False

async def message_callback(msg):
    """Process incoming MQTT message containing a tracking number."""
    try:
        # Assume payload is directly the tracking number as a string
        tracking_number = msg.payload.decode('utf-8') if isinstance(msg.payload, bytes) else msg.payload
        
        # Log the received tracking number
        _LOGGER.info(f"Received tracking number: {tracking_number}")

        # Process the tracking number
    except Exception as e:
        _LOGGER.error(f"Error processing MQTT message: {str(e)}")



async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry):
    """Unload a config entry."""
    unload_ok = await hass.config_entries.async_forward_entry_unload(entry, "sensor")
    if unload_ok:
        subscription = hass.data[DOMAIN][entry.entry_id].get('subscription')
        if subscription:
            await subscription()
        hass.data[DOMAIN].pop(entry.entry_id, None)
    return unload_ok
