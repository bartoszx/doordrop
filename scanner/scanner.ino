#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include "mqtt.h"
#include "secrets.h" // Twój plik z danymi do WiFi i MQTT

#define BARCODE_POWER_PIN D7
#define MOTION_SENSOR_PIN D6
#define LCD_ADDRESS 0x27
#define LCD_COLUMNS 16
#define LCD_ROWS 2
#define MOTION_DELAY_MS 5000 // 5 sekund
#define SCAN_DURATION_MS 10000 // 10 sekund

SoftwareSerial barcodeScanner(D4, D3);
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
    pinMode(MOTION_SENSOR_PIN, INPUT_PULLUP);

    Wire.begin(D2, D1);
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Zeskanuj kod");

    connectToWiFi(); // Połączenie z WiFi
    setupMQTT();
    connectToMQTT(); // Połączenie z MQTT
}

void loop() {
    mqttLoop(); // Obsługa komunikatów MQTT

    if (digitalRead(MOTION_SENSOR_PIN) == HIGH) {
        lastMotionTime = millis();
        if (!motionDetected) {
            digitalWrite(BARCODE_POWER_PIN, HIGH);
            motionDetected = true;
            scanning = true;
            lcd.backlight();
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Zeskanuj kod");
            Serial.println("Zeskanuj kod");
        }
    }

    if (motionDetected && (millis() - lastMotionTime >= MOTION_DELAY_MS)) {
        digitalWrite(BARCODE_POWER_PIN, LOW);
        motionDetected = false;
        scanning = false;
        lcd.noBacklight();
        Serial.println("Ekran i skaner wyłączone z powodu braku ruchu");
    }

    if (scanning && barcodeScanner.available()) {
        String receivedString = barcodeScanner.readStringUntil('\n');
        receivedString.trim(); // Usuwa białe znaki na początku i na końcu
        if (isAlphaNumeric(receivedString)) {
            Serial.print("Received barcode: ");
            Serial.println(receivedString);
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Kod: ");
            lcd.print(receivedString);
            publishToMQTT(receivedString.c_str()); // Publikuj zeskanowany kod do MQTT
            delay(5000); // Wyświetl kod przez 5 sekund
            lcd.setCursor(0, 1); // Ustawienie kursora w drugim wierszu
            lcd.print("Zeskanuj kod"); // Przywracanie komunikatu po wyświetleniu kodu
        }
    }

    delay(100); // Zmniejszyłem opóźnienie, aby szybciej reagować na ruch
}

bool isAlphaNumeric(String str) {
    for (int i = 0; i < str.length(); i++) {
        if (!isAlphaNumeric(str[i])) {
            return false;
        }
    }
    return true;
}
