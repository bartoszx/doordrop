#ifndef MQTT_H
#define MQTT_H

#include <ESPAsyncWebServer.h>

extern bool mqttConnected;
extern AsyncWebServer server; // Declare server as external

void mqttInit(const char* server, int port, const char* user, const char* password, const char* topic);
bool mqttConnect();
void mqttLoop();
void publishToMQTT(const char* message);

#endif
