#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>
#include <time.h>
#include <deque>
#include "mqtt.h"
#include "led_control.h"

String mqttServer;
int mqttPort;
String mqttUser;
String mqttPassword;
String mqttTopic;
String mqttStatusTopic;
String mqttStateTopic;


WiFiClient espClient;
PubSubClient client(espClient);

AsyncWebServer server(80); // Define server here
bool mqttConnected = false;

bool authorized = false;
bool unauthorized = false;

unsigned long lastStatusPublishTime = 0;
const unsigned long statusPublishInterval = 60000; // 60 seconds

extern std::deque<String> errorLogs; // Declare as extern
extern const size_t maxErrorLogs; // Declare as extern
extern void logError(const String &error); // Declare as extern

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

void mqttInit(const char* server, int port, const char* user, const char* password, const char* topic, const char* statusTopic, const char* stateTopic) {
    mqttServer = String(server);
    mqttPort = port;
    mqttUser = String(user);
    mqttPassword = String(password);
    mqttTopic = String(topic);
    mqttStatusTopic = String(statusTopic);
    mqttStateTopic = String(stateTopic);
    client.setServer(mqttServer.c_str(), mqttPort);
    client.setCallback(mqttCallback);
    
    Serial.println("MQTT initialized");
    logError("MQTT initialized");
}




bool mqttConnect() {
    while (!client.connected()) {
        Serial.println("Attempting MQTT connection...");
        logError("Attempting MQTT connection...");

        // Create a random client ID
        String clientId = "ESP8266Client-";
        clientId += String(random(0xffff), HEX);
        
        // Attempt to connect
        if (client.connect(clientId.c_str(), mqttUser.c_str(), mqttPassword.c_str())) {
            Serial.println("MQTT connected");
            logError("MQTT connected");
            // Once connected, publish an announcement...
            publishStatus();
            return true;
        } else {
            String error = "MQTT connect failed, rc=" + String(client.state()) + " try again in 5 seconds";
            Serial.println(error);
            logError(error);
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
    // If the loop exits for any reason, return false
    return false;
}





void checkMqttConnection() {
    if (!client.connected()) {
        mqttConnected = mqttConnect();
    }
}

void mqttLoop() {
    client.loop();

    unsigned long currentMillis = millis();
    if (currentMillis - lastStatusPublishTime >= statusPublishInterval) {
        lastStatusPublishTime = currentMillis;
        publishStatus();
    }
}

void publishToMQTT(const char* message) {
    Serial.print("Publishing to MQTT topic ");
    Serial.print(mqttTopic);
    Serial.print(": ");
    Serial.println(message);
    if (client.publish(mqttTopic.c_str(), message)) {
        Serial.println("Publish successful");
        logError("Publish successful");
    } else {
        Serial.println("Publish failed");
        logError("Publish failed");
    }
}

void publishStatus() {
    // Initialize the time library
    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
    tzset();


    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);

    String statusMessage = "{";
    statusMessage += "\"time\":\"" + String(timeStr) + "\",";
    statusMessage += "\"wifi_status\":\"" + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected") + "\",";
    statusMessage += "\"mqtt_status\":\"" + String(mqttConnected ? "Connected" : "Disconnected") + "\",";
    statusMessage += "\"ip_address\":\"" + WiFi.localIP().toString() + "\"";
    statusMessage += "}";

    Serial.print("Publishing status to MQTT topic ");
    Serial.print(mqttStateTopic);
    Serial.print(": ");
    Serial.println(statusMessage);

    if (client.publish(mqttStateTopic.c_str(), statusMessage.c_str())) {
        Serial.println("Status publish successful");
        logError("Status publish successful: "  + statusMessage);
    } else {
        Serial.println("Status publish failed");
        logError("Status publish failed");
    }
}