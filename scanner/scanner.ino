#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoOTA.h>
#include "mqtt.h"
#include "secrets.h"
#include "led_control.h"

// Pin definitions
#define BARCODE_POWER_PIN D8
#define MOTION_SENSOR_PIN D6
#define LCD_ADDRESS 0x27
#define LCD_COLUMNS 16
#define LCD_ROWS 2
#define LCD_SCL D2
#define LCD_SDA D1
#define BARCODE_TX D3
#define BARCODE_RX D4

// Timing definitions
#define MOTION_DELAY_MS 5000 // 5 seconds
#define SCAN_DURATION_MS 10000 // 10 seconds

// LCD message
#define WELCOME_LCD_MSG "Zeskanuj kod paczki"

// MQTT Topics
#define STATUS_TOPIC "doordrop/status"

// Global variables
SoftwareSerial barcodeScanner(BARCODE_TX, BARCODE_RX);
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);
extern AsyncWebServer server; // Declare server as external
extern bool mqttConnected; // Declare mqttConnected as external

extern bool authorized; // Declare as external
extern bool unauthorized; // Declare as external

unsigned long lastMotionTime = 0;
bool motionDetected = false;
bool scanning = false;

void startAP();
void handleRoot(AsyncWebServerRequest *request);
void handleSave(AsyncWebServerRequest *request);
void handleReset(AsyncWebServerRequest *request);
void setupServer();
void connectToWiFi();
void reconnectWiFiAndMQTT();
void setupOTA();
bool isAlphaNumericStr(String str);

void startAP() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    String macID = String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);
    macID.toUpperCase();
    String apName = "Barcode-" + macID;

    WiFi.softAP(apName.c_str(), "12345678");
    Serial.println("AP Started");
    Serial.print("AP Name: ");
    Serial.println(apName);
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());
}

void handleRoot(AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", String(), false);
}

void handleSave(AsyncWebServerRequest *request) {
    Serial.println("handleSave called");

    if (!LittleFS.begin()) {
        Serial.println("Failed to mount file system");
        request->send(500, "text/plain", "Failed to mount file system.");
        return;
    }

    bool allParamsPresent = request->hasParam("ssid", true) &&
                            request->hasParam("password", true) &&
                            request->hasParam("mqtt_host", true) &&
                            request->hasParam("mqtt_port", true) &&
                            request->hasParam("mqtt_user", true) &&
                            request->hasParam("mqtt_password", true) &&
                            request->hasParam("mqtt_topic", true) &&
                            request->hasParam("mqtt_status_topic", true);

    if (allParamsPresent) {
        String ssid = request->getParam("ssid", true)->value();
        String password = request->getParam("password", true)->value();
        String mqttHost = request->getParam("mqtt_host", true)->value();
        int mqttPort = request->getParam("mqtt_port", true)->value().toInt();
        String mqttUser = request->getParam("mqtt_user", true)->value();
        String mqttPassword = request->getParam("mqtt_password", true)->value();
        String mqttTopic = request->getParam("mqtt_topic", true)->value();
        String mqttStatusTopic = request->getParam("mqtt_status_topic", true)->value();

        Serial.println("Parameters received:");
        Serial.println("SSID: " + ssid);
        Serial.println("Password: " + password);
        Serial.println("MQTT Host: " + mqttHost);
        Serial.println("MQTT Port: " + String(mqttPort));
        Serial.println("MQTT User: " + mqttUser);
        Serial.println("MQTT Password: " + mqttPassword);
        Serial.println("MQTT Topic: " + mqttTopic);
        Serial.println("MQTT Status Topic: " + mqttStatusTopic);

        File file = LittleFS.open("/config.txt", "w");
        if (file) {
            file.println(ssid);
            file.println(password);
            file.println(mqttHost);
            file.println(mqttPort);
            file.println(mqttUser);
            file.println(mqttPassword);
            file.println(mqttTopic);
            file.println(mqttStatusTopic);
            file.close();
            Serial.println("Configuration saved");

            Serial.println("Rebooting...");
            delay(1000);
            ESP.restart();
        } else {
            Serial.println("Failed to open file for writing");
            request->send(500, "text/plain", "Failed to open file for writing.");
        }
    } else {
        Serial.println("Missing parameters");
        request->send(400, "text/plain", "Missing parameters.");
    }
}

void handleReset(AsyncWebServerRequest *request) {
    Serial.println("handleReset called");

    if (LittleFS.exists("/config.txt")) {
        LittleFS.remove("/config.txt");
        Serial.println("Configuration file deleted");
    } else {
        Serial.println("Configuration file not found");
    }

    request->send(200, "text/plain", "Configuration reset. Rebooting...");
    delay(1000);
    ESP.restart();
}

void setupServer() {
    if (!LittleFS.begin()) {
        Serial.println("Failed to mount file system");
        return;
    }

    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/reset", HTTP_GET, handleReset);
    server.begin();
}

void connectToWiFi() {
    if (!LittleFS.begin()) {
        Serial.println("Failed to mount file system");
        return;
    }

    File file = LittleFS.open("/config.txt", "r");
    if (!file) {
        Serial.println("No configuration found, starting AP...");
        startAP();
        setupServer();
        return;
    }

    String ssid = file.readStringUntil('\n');
    String password = file.readStringUntil('\n');
    String mqttHost = file.readStringUntil('\n');
    String mqttPortStr = file.readStringUntil('\n');
    String mqttUser = file.readStringUntil('\n');
    String mqttPassword = file.readStringUntil('\n');
    String mqttTopic = file.readStringUntil('\n');
    String mqttStatusTopic = file.readStringUntil('\n');
    file.close();

    ssid.trim();
    password.trim();
    mqttHost.trim();
    mqttPortStr.trim();
    mqttUser.trim();
    mqttPassword.trim();
    mqttTopic.trim();
    mqttStatusTopic.trim();

    int mqttPort = mqttPortStr.toInt();

    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
        delay(1000);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to WiFi");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());

        setupServer(); // Start the server on the target network

        mqttInit(mqttHost.c_str(), mqttPort, mqttUser.c_str(), mqttPassword.c_str(), mqttTopic.c_str(), mqttStatusTopic.c_str());
        mqttConnected = mqttConnect();
        if (!mqttConnected) {
            Serial.println("Failed to connect to MQTT");
            reconnectWiFiAndMQTT();
        }
    } else {
        Serial.println("\nFailed to connect to WiFi, starting AP...");
        startAP();
        setupServer();
    }
}

void reconnectWiFiAndMQTT() {
    while (WiFi.status() != WL_CONNECTED || !mqttConnected) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("Reconnecting to WiFi...");
            WiFi.reconnect();
        }
        if (!mqttConnected) {
            Serial.println("Reconnecting to MQTT...");
            mqttConnected = mqttConnect();
        }
        delay(5000); // Wait before retrying
    }
    Serial.println("Reconnected to WiFi and MQTT");
}

void setupOTA() {
    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else { // U_SPIFFS
            type = "filesystem";
        }
        Serial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
            Serial.println("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            Serial.println("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            Serial.println("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            Serial.println("Receive Failed");
        } else if (error == OTA_END_ERROR) {
            Serial.println("End Failed");
        }
    });
    ArduinoOTA.begin();
}

void setup() {
    Serial.begin(9600);
    barcodeScanner.begin(9600);
    pinMode(BARCODE_POWER_PIN, OUTPUT);
    pinMode(MOTION_SENSOR_PIN, INPUT);

    // Initialize LED strip
    initLED();

    Wire.begin(LCD_SCL, LCD_SDA);
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Connecting...");

    connectToWiFi();
    setupOTA();
}

void loop() {
    if (mqttConnected) {
        mqttLoop(); // MQTT message handling

        if (digitalRead(MOTION_SENSOR_PIN) == HIGH) {
            lastMotionTime = millis();
            if (!motionDetected) {
                digitalWrite(BARCODE_POWER_PIN, HIGH);
                motionDetected = true;
                scanning = true;
                lcd.backlight();
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print(WELCOME_LCD_MSG); // Scan code
                Serial.println("Scan code");
            }
        }

        if (motionDetected && (millis() - lastMotionTime >= MOTION_DELAY_MS)) {
            digitalWrite(BARCODE_POWER_PIN, LOW);
            motionDetected = false;
            scanning = false;
            lcd.noBacklight();
            Serial.println("Screen and scanner turned off due to no motion");
        }

        if (scanning && barcodeScanner.available()) {
            String receivedString = barcodeScanner.readStringUntil('\n');
            receivedString.trim(); // Removes white space at the beginning and end
            if (isAlphaNumericStr(receivedString)) {
                Serial.print("Received barcode: ");
                Serial.println(receivedString);
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Kod: ");
                lcd.print(receivedString);

                // Reset the authorization status flags
                authorized = false;
                unauthorized = false;
                Serial.println("Reset authorized and unauthorized flags");

                // Publish the scanned barcode to MQTT
                if (mqttConnected) {
                    publishToMQTT(receivedString.c_str());
                }

                // Wait for the authorization result
                unsigned long startWaitTime = millis();
                while ((millis() - startWaitTime < 5000) && !authorized && !unauthorized) {
                    mqttLoop();
                    delay(100);
                }

                // Check the authorization status and update the LED accordingly
                if (authorized) {
                    setLEDColor("green");
                    Serial.println("LED color set to green");
                } else if (unauthorized) {
                    setLEDColor("red");
                    Serial.println("LED color set to red");
                } else {
                    setLEDColor("white"); // No status received, default to white
                    Serial.println("LED color set to white");
                }

                delay(5000); // Display code and LED color for 5 seconds

                // Reset LED color to white after 5 seconds
                setLEDColor("white");
                Serial.println("LED color reset to white");

                lcd.setCursor(0, 1); // Set cursor in the second row
                lcd.print(WELCOME_LCD_MSG); // Restore message after displaying code
            }
        }




    }
    ArduinoOTA.handle();
    delay(100); // Reduced delay to respond faster to motion
}

bool isAlphaNumericStr(String str) {
    for (int i = 0; i < str.length(); i++) {
        if (!isAlphaNumeric(str[i])) {
            return false;
        }
    }
    return true;
}
