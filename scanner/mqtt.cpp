#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>
#include "mqtt.h"

const char* mqttServer;
int mqttPort;
const char* mqttUser;
const char* mqttPassword;
String mqttTopic;

WiFiClient espClient;
PubSubClient client(espClient);

AsyncWebServer server(80); // Define server here
bool mqttConnected = false;

void mqttInit(const char* server, int port, const char* user, const char* password, const char* topic) {
    mqttServer = server;
    mqttPort = port;
    mqttUser = user;
    mqttPassword = password;
    mqttTopic = String(topic);
    client.setServer(mqttServer, mqttPort);
    Serial.println("MQTT initialized");
}

bool mqttConnect() {
    Serial.println("Connecting to MQTT...");
    if (client.connect("ESP8266Client", mqttUser, mqttPassword)) {
        Serial.println("MQTT connected");
        return true;
    } else {
        Serial.print("MQTT connect failed, rc=");
        Serial.println(client.state());
        return false;
    }
}

void mqttLoop() {
    client.loop();
}

void publishToMQTT(const char* message) {
    Serial.print("Publishing to MQTT topic ");
    Serial.print(mqttTopic);
    Serial.print(": ");
    Serial.println(message);
    if (client.publish(mqttTopic.c_str(), message)) {
        Serial.println("Publish successful");
    } else {
        Serial.println("Publish failed");
    }
}
