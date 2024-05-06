from asyncio import gather

from homeassistant import bootstrap
from homeassistant.helpers.entity_platform import async_get_platforms


def get_platform(hass, name):
    platform_list = async_get_platforms(hass, name)

    for platform in platform_list:
        if platform.domain == name:
            return platform

    return None


def create_platform(hass, name):
    gather(
        bootstrap.async_setup_component(
            hass,
            name,
            {}
        )
    )