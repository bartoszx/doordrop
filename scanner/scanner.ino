#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoOTA.h>
#include "mqtt.h"
#include <time.h>
#include "secrets.h"
#include "led_control.h"
#include <deque>

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
#define MOTION_DELAY_MS 25000 // 25 seconds
#define SCAN_DURATION_MS 10000 // 10 seconds

// LCD message
#define WELCOME_LCD_MSG "Zeskanuj paczke"

// MQTT Topics
#define STATUS_TOPIC "doordrop/status"
#define STATE_TOPIC "doordrop/state"

// Global variables
SoftwareSerial barcodeScanner(BARCODE_TX, BARCODE_RX);
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);
extern AsyncWebServer server; // Declare server as external
extern bool mqttConnected; // Declare mqttConnected as external

extern bool authorized; // Declare as external
extern bool unauthorized; // Declare as external

std::deque<String> errorLogs;
const size_t maxErrorLogs = 50; // Maximum number of error logs to store

unsigned long lastMotionTime = 0;
bool motionDetected = false;
bool scanning = false;

void startAP();
void handleRoot(AsyncWebServerRequest *request);
void handleSave(AsyncWebServerRequest *request);
void handleReset(AsyncWebServerRequest *request);
void handleLogs(AsyncWebServerRequest *request);
void handleStatus(AsyncWebServerRequest *request);
void setupServer();
void connectToWiFi();
void reconnectWiFiAndMQTT();
void setupOTA();
bool isAlphaNumericStr(String str);
void logError(const String &error);

void logError(const String &error) {
    if (errorLogs.size() >= maxErrorLogs) {
        errorLogs.pop_front(); // Remove oldest log if max size is reached
    }
    errorLogs.push_back(error);
    Serial.println(error); // Also print to Serial for debugging
}

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
    logError("Started AP: " + apName + " with IP: " + WiFi.softAPIP().toString());
}

void handleRoot(AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", String(), false);
}

void handleSave(AsyncWebServerRequest *request) {
    Serial.println("handleSave called");

    if (!LittleFS.begin()) {
        logError("Failed to mount file system");
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
            logError("Configuration saved");

            Serial.println("Rebooting...");
            delay(1000);
            ESP.restart();
        } else {
            logError("Failed to open file for writing");
            request->send(500, "text/plain", "Failed to open file for writing.");
        }
    } else {
        logError("Missing parameters");
        request->send(400, "text/plain", "Missing parameters.");
    }
}

void handleReset(AsyncWebServerRequest *request) {
    Serial.println("handleReset called");

    if (LittleFS.exists("/config.txt")) {
        LittleFS.remove("/config.txt");
        Serial.println("Configuration file deleted");
        logError("Configuration file deleted");
    } else {
        Serial.println("Configuration file not found");
        logError("Configuration file not found");
    }

    request->send(200, "text/plain", "Configuration reset. Rebooting...");
    delay(1000);
    ESP.restart();
}

void handleLogs(AsyncWebServerRequest *request) {
    String logPage = "<html><body><h1>Error Logs</h1><pre>";
    for (const String &log : errorLogs) {
        logPage += log + "\n";
    }
    logPage += "</pre></body></html>";
    request->send(200, "text/html", logPage);
}

void handleStatus(AsyncWebServerRequest *request) {
    String statusPage = "<html><body><h1>System Status</h1><pre>";
    statusPage += "WiFi SSID: " + String(WiFi.SSID()) + "\n";
    statusPage += "WiFi Status: " + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected") + "\n";
    statusPage += "MQTT Status: " + String(mqttConnected ? "Connected" : "Disconnected") + "\n";
    statusPage += "IP Address: " + WiFi.localIP().toString() + "\n";
    statusPage += "</pre></body></html>";
    request->send(200, "text/html", statusPage);
}

void setupServer() {
    if (!LittleFS.begin()) {
        Serial.println("Failed to mount file system");
        logError("Failed to mount file system");
        return;
    }

    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/reset", HTTP_GET, handleReset);
    server.on("/logs", HTTP_GET, handleLogs); // Add this line
    server.on("/status", HTTP_GET, handleStatus); // Add this line
    server.begin();
}

void connectToWiFi() {
    if (!LittleFS.begin()) {
        Serial.println("Failed to mount file system");
        logError("Failed to mount file system");
        return;
    }

    File file = LittleFS.open("/config.txt", "r");
    if (!file) {
        Serial.println("No configuration found, starting AP...");
        logError("No configuration found, starting AP...");
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

    // Trim whitespace from the ends
    ssid.trim();
    password.trim();
    mqttHost.trim();
    mqttPortStr.trim();
    mqttUser.trim();
    mqttPassword.trim();
    mqttTopic.trim();
    mqttStatusTopic.trim();

    // Convert mqttPortStr to integer
    int mqttPort = mqttPortStr.toInt();

    // Log the read variables
    Serial.print("Read SSID: ");
    Serial.println(ssid);
    Serial.print("Read Password: ");
    Serial.println(password);
    Serial.print("Read MQTT Host: ");
    Serial.println(mqttHost);
    Serial.print("Read MQTT Port: ");
    Serial.println(mqttPort);
    Serial.print("Read MQTT User: ");
    Serial.println(mqttUser);
    Serial.print("Read MQTT Password: ");
    Serial.println(mqttPassword);
    Serial.print("Read MQTT Topic: ");
    Serial.println(mqttTopic);
    Serial.print("Read MQTT Status Topic: ");
    Serial.println(mqttStatusTopic);

    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.persistent(true);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long startAttemptTime = millis();

    // Wait for connection to WiFi
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 20000) {
        delay(1000);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to WiFi");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        logError("Connected to WiFi with IP: " + WiFi.localIP().toString());

        setupServer(); // Start the server on the target network

        mqttInit(mqttHost.c_str(), mqttPort, mqttUser.c_str(), mqttPassword.c_str(), mqttTopic.c_str(), mqttStatusTopic.c_str(), STATE_TOPIC); // Add state topic argument

        // Retry MQTT connection until successful
        while (!mqttConnected) {
            mqttConnected = mqttConnect();
            if (!mqttConnected) {
                delay(5000);
            }
        }
        logError("Connected to MQTT");
    } else {
        logError("Failed to connect to WiFi, starting AP...");
        startAP();
        setupServer();
    }
}




void reconnectWiFiAndMQTT() {
    while (WiFi.status() != WL_CONNECTED || !mqttConnected) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("Reconnecting to WiFi...");
            logError("Reconnecting to WiFi...");
            WiFi.reconnect();
        }
        if (!mqttConnected) {
            Serial.println("Reconnecting to MQTT...");
            logError("Reconnecting to MQTT...");
            mqttConnected = mqttConnect();
        }
        delay(5000); // Wait before retrying
    }
    Serial.println("Reconnected to WiFi and MQTT");
    logError("Reconnected to WiFi and MQTT");
}

void setupOTA() {
    // Ustaw hasło OTA, jeśli chcesz je zabezpieczyć
    const char* otaPassword = "123456"; // Zmień na swoje hasło, jeśli chcesz zabezpieczenia

    ArduinoOTA.setPassword(otaPassword);

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
            logError("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            logError("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            logError("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            logError("Receive Failed");
        } else if (error == OTA_END_ERROR) {
            logError("End Failed");
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

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    // Set the time zone to CET
    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
    tzset();


    connectToWiFi();
    setupOTA();

    logError("Setup completed");

}

void loop() {

    checkMqttConnection(); // Ensure MQTT connection

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

                // Clear the LCD and restore the welcome message
                lcd.clear();
                lcd.setCursor(0, 0); // Set cursor in the first row
                lcd.print(WELCOME_LCD_MSG);
                Serial.println("Restored welcome message on LCD");
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
