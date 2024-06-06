#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>
#include <time.h>
#include <deque>
#include "mqtt.h"
#include "led_control.h"

const char* mqttServer;
int mqttPort;
const char* mqttUser;
const char* mqttPassword;
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

void logError(const String &error) {
    if (errorLogs.size() >= maxErrorLogs) {
        errorLogs.pop_front(); // Remove oldest log if max size is reached
    }
    errorLogs.push_back(error);
    Serial.println(error); // Also print to Serial for debugging
}

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
    mqttServer = server;
    mqttPort = port;
    mqttUser = user;
    mqttPassword = password;
    mqttTopic = String(topic);
    mqttStatusTopic = String(statusTopic);
    mqttStateTopic = String(stateTopic);
    client.setServer(mqttServer, mqttPort);
    client.setCallback(mqttCallback); // Set the callback function
    Serial.println("MQTT initialized");
    logError("MQTT initialized");
}

bool mqttConnect() {
    Serial.println("Connecting to MQTT...");
    logError("Connecting to MQTT...");
    if (client.connect("ESP8266Client", mqttUser, mqttPassword)) {
        Serial.println("MQTT connected");
        logError("MQTT connected");
        client.subscribe(mqttStatusTopic.c_str()); // Subscribe to the status topic
        return true;
    } else {
        String error = "MQTT connect failed, rc=" + String(client.state());
        Serial.println(error);
        logError(error);
        return false;
    }
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
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    time_t now = time(nullptr);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    
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
        logError("Status publish successful");
    } else {
        Serial.println("Status publish failed");
        logError("Status publish failed");
    }
}
