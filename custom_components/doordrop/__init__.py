from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.components.mqtt import async_subscribe
from .const import AUTHORIZED_BARCODES
import logging
import asyncio

# Import the sensor platform to ensure it's loaded
from . import sensor

_LOGGER = logging.getLogger(__name__)
DOMAIN = "doordrop"
PLATFORMS = ["sensor"]

async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry):
    """Setup config entry for Home Assistant."""
    _LOGGER.debug("Setting up DoorDrop component.")
    _LOGGER.debug("Entry data: %s", entry.data)

    hass.data.setdefault(DOMAIN, {})
    hass.data[DOMAIN][entry.entry_id] = {
        'config': dict(entry.data)
    }

    _LOGGER.debug("Forwarding entry setup to sensor platform...")
    try:
        result = await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)
        _LOGGER.debug("async_forward_entry_setups result: %s", result)
        if result is False:  # Only fail if explicitly False, not None
            _LOGGER.error("Failed to forward entry setup to sensor platform.")
            return False
    except Exception as e:
        _LOGGER.error("Exception during async_forward_entry_setups: %s", str(e), exc_info=True)
        return False

    _LOGGER.debug("DoorDrop component setup complete.")
    return True

async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry):
    """Unload a config entry."""
    unload_ok = await hass.config_entries.async_forward_entry_unloads(entry, PLATFORMS)
    if unload_ok:
        hass.data[DOMAIN].pop(entry.entry_id, None)
    return unload_ok

def is_authorized_barcode(barcode, authorized_barcodes):
    return barcode in authorized_barcodes.split(",")