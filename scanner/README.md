# DoorDrop ESP32 Scanner Module

## Overview
This module is designed to work with the DoorDrop Home Assistant integration. It is an IoT device based on the ESP32 platform that scans barcodes and communicates with the Home Assistant system via MQTT. The scanner module ensures secure and automated gate operation for package deliveries.

## Features

- **Barcode Scanning**: Scans barcodes using a connected scanner module.
- **MQTT Communication**: Sends scanned codes to Home Assistant for verification.
- **WiFi Connectivity**: Connects to your home WiFi network for internet access.
- **Real-time Feedback**: Provides real-time status updates based on the verification of scanned codes.
- **AP Mode Configuration**: Configure WiFi and MQTT settings via a web interface in AP mode.
- **OTA Updates**: Support for Over-The-Air (OTA) updates for remote firmware upgrades.

## Prerequisites
To use the DoorDrop ESP32 Scanner Module, you need:

- An ESP32 D1 mini WiFi development board.
- A barcode scanner (EV-ER08 1D).
- A power module (WP1584 3A or XL4016).
- A 5-LED strip (WS2812B, 12V).
- A Microwave Radar Sensor (HFS-DC06H).
- An LCD screen (PCF8574).
- Access to a WiFi network.
- An MQTT broker (configured as part of the Home Assistant setup).

## Installation

### Hardware Setup
1. **Connect the Barcode Scanner**: Connect the EV-ER08 1D barcode scanner to the ESP32 board according to the wiring diagram provided.
2. **Connect the Power Module**: Use the WP1584 3A (or XL4016) power module to regulate the power supply to the ESP32 and peripherals.
3. **Connect the LED Strip**: Connect the 5-LED WS2812B strip to the ESP32.
4. **Connect the Radar Sensor**: Connect the HFS-DC06H Microwave Radar Sensor to the ESP32.
5. **Connect the LCD Screen**: Connect the LCD screen using the PCF8574 module.
6. **Power the ESP32**: Ensure the ESP32 is powered via USB or an appropriate power source.

### Software Setup
1. **Clone the Repository**: Clone this repository to your local machine.
    ```bash
    git clone https://github.com/bartoszx/doordrop.git
    cd doordrop
    ```

2. **Open the Project**: Open the project in your preferred IDE (e.g., Arduino IDE, PlatformIO).

3. **Install Libraries**: Ensure you have the required libraries installed. This typically includes libraries for WiFi, MQTT, the barcode scanner, WS2812B LEDs, and the LCD screen.

4. **Upload the Code**: Upload the code to your ESP32 board using the IDE.

## Configuration

### AP Mode Configuration
1. **Initial Setup**: After uploading the firmware, the ESP32 will start in AP mode.
2. **Connect to AP**: Connect to the WiFi network named `DoorDrop-Setup` using your phone or computer.
3. **Open Configuration Page**: Open a web browser and navigate to `192.168.4.1`.
4. **Enter Details**: Enter your WiFi credentials, MQTT broker details, and topics for the scanner.
    - **WiFi SSID**: Your WiFi network name.
    - **WiFi Password**: Your WiFi network password.
    - **MQTT Broker**: The address of your MQTT broker.
    - **MQTT Topic**: The topic to which the ESP32 will publish scanned codes.
    - **MQTT Status Topic**: The topic for status updates.

### OTA Updates
1. **Trigger OTA Update**: Access the configuration page or use an MQTT command to trigger the OTA update process.
2. **Upload New Firmware**: Upload the new firmware through the web interface.

## Usage

### Normal Operation
1. **Power the Device**: Ensure the ESP32 board is powered on.
2. **Scan a Barcode**: Use the connected barcode scanner to scan a delivery code.
3. **Status Updates**: The ESP32 module will send the scanned code to Home Assistant via MQTT. Home Assistant will verify the code and send back a status update.

### MQTT Communication
- **Scanned Code**: The ESP32 sends the scanned barcode to the MQTT topic configured during setup.
- **Status Updates**: Home Assistant processes the barcode and sends back a status update to the MQTT status topic:
  - `"Authorized"` if the code is verified.
  - `"Unauthorized"` if the code is not verified.
- **Indicator Feedback**: The LED strip provides visual feedback based on the status:
  - Green LEDs for authorized codes.
  - Red LEDs for unauthorized codes.

### Debugging
For troubleshooting, use the serial monitor in your IDE to check for any error messages or status updates.

## 3D Model for Enclosure
A 3D model for the enclosure of the ESP32 scanner module is available in the `fusion360/` directory. This can be used to print a custom enclosure to house the ESP32 and barcode scanner module.

## Contributing

Contributions to the DoorDrop ESP32 Scanner Module are welcome! Here's how you can contribute:

1. **Fork the Project**
2. **Create your Feature Branch** (`git checkout -b feature/AmazingFeature`)
3. **Commit your Changes** (`git commit -m 'Add some AmazingFeature'`)
4. **Push to the Branch** (`git push origin feature/AmazingFeature`)
5. **Open a Pull Request**

## License

Distributed under the MIT License. See `LICENSE` file for more information.

## Contact

Project Link: [https://github.com/bartoszx/doordrop](https://github.com/bartoszx/doordrop)
