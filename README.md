
# DoorDrop - Home Assistant Integration
## Overview
DoorDrop is a custom Home Assistant integration designed to automate the process of verifying delivery codes and controlling gate access for package deliveries. By utilizing email scanning, MQTT communication, and a MySQL database, DoorDrop ensures secure and automated gate operation.
  
## Features

-  **Email Scanning:** Extracts delivery codes automatically from incoming emails.
-  **MQTT Integration:** Communicates with IoT devices via MQTT for real-time control.
-  **Database Support:** Manages delivery codes and their verification statuses.
-  **Automated Gate Control:** Opens gates automatically for verified deliveries.
  
## Prerequisites
To use the DoorDrop integration, you need:

- A running instance of Home Assistant.
- An MQTT broker configured and running in Home Assistant.
- Access to a MySQL database.

## Installation

To install DoorDrop:

1.  **Clone the Repository:**
Navigate to your Home Assistant configuration directory under `custom_components`. Clone this repository there:
```bash
cd /path/to/your/homeassistant/config/custom_components
git clone https://github.com/bartoszx/doordrop.git
```
2. **Restart Home Assistant:**
Restart Home Assistant to load the new integration.

## Configuration

Configure DoorDrop via the Home Assistant UI:

1.  Go to **Settings** > **Integrations**.
2.  Click **Add Integration**.
3.  Search for **DoorDrop** and follow the on-screen instructions to configure it with details like your email, database, and MQTT settings.
## Usage

Once DoorDrop is set up, it will start monitoring specified email addresses for delivery codes, verify them against your database, and use MQTT commands to control gates or other devices when deliveries are verified.

### Debugging

For troubleshooting, check the Home Assistant logs through **Settings** > **Logs** if you encounter issues related to email scanning or MQTT communication.

## Contributing

Contributions to DoorDrop are welcome! Here's how you can contribute:

1.  **Fork the Project**
2.  **Create your Feature Branch** (`git checkout -b feature/AmazingFeature`)
3.  **Commit your Changes** (`git commit -m 'Add some AmazingFeature'`)
4.  **Push to the Branch** (`git push origin feature/AmazingFeature`)
5.  **Open a Pull Request**

## License

Distributed under the MIT License. See `LICENSE` file for more information.

## Contact


Project Link: [https://github.com/yourusername/doordrop](https://github.com/bartoszx/doordrop)


### Changes Made:
- **Installation**: Simplified to emphasize cloning into `custom_components`.
- **Configuration**: Updated to reflect UI-based configuration through Home Assistant.
- **General Formatting**: Adjusted to improve readability and clarity.


UZUEPLNIENIA
- ze w pliku patterns.py sa patterny do numerow trackingowych
- ze poprawnie zmatchowany numer paczki jest dodawany do bazy wraz z data dodania
- ze kurier moze podjac dwie proby w 30s przeslania numeru (w nawieasie dodac ze warto dodac jako parametr todo)
- poprawnie odebrany numer zostanie zdeaktywowany i wskzana data deaktywacji 
- jesli numer przesylki jest odnaleziony w bazie to wywoluje input_button.press i nalezy dodac input_button a jego nazwe wpisac w konfiguracji nastepnie prosta automatyzacja mozna otwierac furkte czyt wrzutke

- id: '1659731095265'
  alias: otwórz furtkę script call
  description: ''
  trigger:
  - platform: state
    entity_id:
    - input_button.gate
  condition: []
  action:
  - service: script.gate_cancel_ring_send_voice
    data: {}