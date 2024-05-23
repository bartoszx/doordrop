#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "mqtt.h"
#include "secrets.h" // Twój plik z danymi do WiFi i MQTT
#include <ArduinoJson.h>

WiFiClient espClient;
PubSubClient mqttClient(espClient);

void setupMQTT() {
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
}

void connectToMQTT() {
    while (!mqttClient.connected()) {
        Serial.print("Connecting to MQTT...");
        String clientId = "ESP8266Client-" + String(ESP.getChipId());
        if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
            Serial.println("connected");
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

void publishToMQTT(const char* message) {
    if (mqttClient.connected()) {
        mqttClient.publish(MQTT_TOPIC, message);
        Serial.println("Published to MQTT: " + String(message));
    } else {
        Serial.println("Not connected to MQTT server");
    }
}

void mqttLoop() {
    mqttClient.loop();
}
