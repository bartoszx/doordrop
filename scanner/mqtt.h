#ifndef MQTT_H
#define MQTT_H

#include <ESPAsyncWebServer.h>
#include <deque>

extern AsyncWebServer server;
extern bool mqttConnected;

extern bool authorized;
extern bool unauthorized;

extern std::deque<String> errorLogs; // Declare as extern
extern const size_t maxErrorLogs; // Declare as extern

void mqttInit(const char* server, int port, const char* user, const char* password, const char* topic, const char* statusTopic, const char* stateTopic);
bool mqttConnect();
void mqttLoop();
void publishToMQTT(const char* message);
void publishStatus();
void checkMqttConnection(); // Add this declaration
extern void logError(const String &error); // Declare as extern

#endif
