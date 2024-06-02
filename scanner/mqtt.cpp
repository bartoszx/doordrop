#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>
#include "mqtt.h"
#include "led_control.h"

const char* mqttServer;
int mqttPort;
const char* mqttUser;
const char* mqttPassword;
String mqttTopic;
String mqttStatusTopic;

WiFiClient espClient;
PubSubClient client(espClient);

AsyncWebServer server(80); // Define server here
bool mqttConnected = false;

bool authorized = false;
bool unauthorized = false;

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    String message = "";
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.println(message);

    // Process the message
    if (String(topic) == mqttStatusTopic) {
        if (message == "Authorized") {
            authorized = true;
            unauthorized = false;
            Serial.println("Set authorized to true and unauthorized to false");
        } else if (message == "Unauthorized") {
            authorized = false;
            unauthorized = true;
            Serial.println("Set authorized to false and unauthorized to true");
        }
    }
}


void mqttInit(const char* server, int port, const char* user, const char* password, const char* topic, const char* statusTopic) {
    mqttServer = server;
    mqttPort = port;
    mqttUser = user;
    mqttPassword = password;
    mqttTopic = String(topic);
    mqttStatusTopic = String(statusTopic);
    client.setServer(mqttServer, mqttPort);
    client.setCallback(mqttCallback); // Set the callback function
    Serial.println("MQTT initialized");
}

bool mqttConnect() {
    Serial.println("Connecting to MQTT...");
    if (client.connect("ESP8266Client", mqttUser, mqttPassword)) {
        Serial.println("MQTT connected");
        client.subscribe(mqttStatusTopic.c_str()); // Subscribe to the status topic
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
