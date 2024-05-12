#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include "mqtt.h"
#include "secrets.h" // Your file with WiFi and MQTT credentials

#define BARCODE_POWER_PIN D8
#define MOTION_SENSOR_PIN D6
#define LCD_ADDRESS 0x27
#define LCD_COLUMNS 16
#define LCD_ROWS 2
#define MOTION_DELAY_MS 5000 // 5 seconds
#define SCAN_DURATION_MS 10000 // 10 seconds
#define LCD_SCL D2
#define LCD_SDA D1
#define BARCODE_TX D3
#define BARCODE_RX D4
#define WELCOME_LCD_MSG "Zeskanuj kod paczki"

SoftwareSerial barcodeScanner(BARCODE_TX, BARCODE_RX);
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

unsigned long lastMotionTime = 0;
bool motionDetected = false;
bool scanning = false;

void connectToWiFi() {
    Serial.print("Connecting to ");
    Serial.println(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi");
}

void setup() {
    Serial.begin(9600);
    barcodeScanner.begin(9600);
    pinMode(BARCODE_POWER_PIN, OUTPUT);
    pinMode(MOTION_SENSOR_PIN, INPUT);

    Wire.begin(LCD_SCL, LCD_SDA);
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Scan code");

    connectToWiFi(); // Connect to WiFi
    setupMQTT();
    connectToMQTT(); // Connect to MQTT
}

void loop() {
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
            lcd.print(WELCOME_LCD_MSG); //Scan code
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
        if (isAlphaNumeric(receivedString)) {
            Serial.print("Received barcode: ");
            Serial.println(receivedString);
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Kod: ");
            lcd.print(receivedString);
            publishToMQTT(receivedString.c_str()); // Publish scanned code to MQTT
            delay(5000); // Display code for 5 seconds
            lcd.setCursor(0, 1); // Set cursor in the second row
            lcd.print(WELCOME_LCD_MSG); // Restore message after displaying code
        }
    }

    delay(100); // Reduced delay to respond faster to motion
}

bool isAlphaNumeric(String str) {
    for (int i = 0; i < str.length(); i++) {
        if (!isAlphaNumeric(str[i])) {
            return false;
        }
    }
    return true;
}
