import re
import logging

_LOGGER = logging.getLogger(__name__)

def find_tracking_code(provider, body, patterns):
    """Find the tracking code for a specific provider using defined patterns."""
    if provider not in patterns:
        _LOGGER.warning("No patterns found for provider %s", provider)
        return None

    provider_patterns = patterns[provider]
    if not isinstance(provider_patterns, list):
        provider_patterns = [provider_patterns]

    for pattern in provider_patterns:
        if match := re.search(pattern, body):
            _LOGGER.info(f"Found tracking code for {provider}: {match.group()}")
            return match.group()
    
    _LOGGER.warning(f"No tracking code found for provider {provider} in the provided body")
    return None
