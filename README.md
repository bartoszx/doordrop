# DoorDrop - Home Assistant Integration

## Overview
DoorDrop is a custom Home Assistant integration designed to automate the process of verifying delivery codes and controlling gate access for package deliveries. By utilizing email scanning, MQTT communication, and a MySQL database, DoorDrop ensures secure and automated gate operation.

## Features

- **Email Scanning:** Extracts delivery codes automatically from incoming emails.
- **MQTT Integration:** Communicates with IoT devices via MQTT for real-time control.
- **Database Support:** Manages delivery codes and their verification statuses.
- **Automated Gate Control:** Opens gates automatically for verified deliveries.

## Prerequisites
To use the DoorDrop integration, you need:

- A running instance of Home Assistant.
- An MQTT broker configured and running in Home Assistant.
- Access to a MySQL database.
- An IoT device (barcode scanner) configured to communicate with the DoorDrop integration. Detailed instructions can be found in the `scanner` directory within the repository.

![](https://www.dropbox.com/s/mjzk9frnqgk7bv0/IMG_8169.jpeg?dl=0&raw=1)
![](https://www.dropbox.com/scl/fi/iiklwqu6oqruxzwgi2u1v/IMG_8167.jpeg?rlkey=55nu43rh9skxs68jxxz8xrsbg&dl=0&raw=1)

## Installation

To install DoorDrop:

1. **Clone the Repository:** Navigate to your Home Assistant configuration directory under `custom_components`. Clone this repository there:
    ```bash
    cd /path/to/your/homeassistant/config/custom_components
    git clone https://github.com/bartoszx/doordrop.git
    ```
2. **Restart Home Assistant:** Restart Home Assistant to load the new integration.

## Configuration

Configure DoorDrop via the Home Assistant UI:

1. Go to **Settings** > **Integrations**.
2. Click **Add Integration**.
3. Search for **DoorDrop** and follow the on-screen instructions to configure it with details like your email, database, MQTT settings, and a list of authorized barcodes.

## Usage

Once DoorDrop is set up, it will start monitoring specified email addresses for delivery codes, verify them against your database, and use MQTT commands to control gates or other devices when deliveries are verified.

### Additional Information

- **Patterns for Tracking Numbers:** In the `patterns.py` file, there are patterns used for identifying tracking numbers from different couriers.
- **Adding to Database:** A correctly matched package number is added to the database along with the date it was added.
- **Multiple Attempts:** A courier can attempt to send the number twice within 30 seconds (this parameter can be adjusted).
- **Deactivation of Numbers:** A correctly received number will be deactivated and the deactivation date will be recorded.
- **MQTT Communication:** 
  - When a scanned code is received via MQTT, it is processed and checked if it's authorized.
  - If authorized, the sensor state is updated to "authorized" and a message "Authorized" is published to the MQTT status topic.
  - If not authorized, the sensor state is updated to "unauthorized" and a message "Unauthorized" is published to the MQTT status topic.
- **Authorized Barcodes List:** During installation, you can define a list of barcodes that will always be authorized.
- **Automation Example:** When a shipment number is found in the database, it triggers an automation. Here is an example based on the sensor:
  
    ```yaml
        alias: DoorDrop Autoryzacja
        description: DoorDrop Autoryzacja
        trigger:
          - platform: state
            entity_id: sensor.shipment_tracker
        condition: []
        action:
          - choose:
              - conditions:
                  - condition: state
                    entity_id: sensor.shipment_tracker
                    state: authorized
                sequence:
                  - service: dahua_vto.open_door
                    data:
                      entity_id: sensor.furtka
                      channel: 1
                      short_number: HA
                  - service: dahua_vto.send_command
                    data:
                      entity_id: sensor.furtka
                      method: console.runCmd
                      params:
                        command: hc
                  - delay: "00:00:02"
                  - service: shell_command.play_autoryzacja
                    data: {}
              - conditions:
                  - condition: state
                    entity_id: sensor.shipment_tracker
                    state: unauthorized
                sequence:
                  - service: shell_command.play_nonautoryzacja
                    data: {}
                  - service: dahua_vto.send_command
                    data:
                      entity_id: sensor.furtka
                      method: console.runCmd
                      params:
                        command: call 9901
                      event: false
        mode: single

    ```

### Debugging

For troubleshooting, check the Home Assistant logs through **Settings** > **Logs** if you encounter issues related to email scanning or MQTT communication.

## Contributing

Contributions to DoorDrop are welcome! Here's how you can contribute:

1. **Fork the Project**
2. **Create your Feature Branch** (`git checkout -b feature/AmazingFeature`)
3. **Commit your Changes** (`git commit -m 'Add some AmazingFeature'`)
4. **Push to the Branch** (`git push origin feature/AmazingFeature`)
5. **Open a Pull Request**

## License

Distributed under the MIT License. See `LICENSE` file for more information.

## Contact

Project Link: [https://github.com/bartoszx/doordrop](https://github.com/bartoszx/doordrop)
