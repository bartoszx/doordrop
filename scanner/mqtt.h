#ifndef MQTT_H
#define MQTT_H

#include <ESPAsyncWebServer.h>

extern AsyncWebServer server;
extern bool mqttConnected;

extern bool authorized;
extern bool unauthorized;

void mqttInit(const char* server, int port, const char* user, const char* password, const char* topic, const char* statusTopic);
bool mqttConnect();
void mqttLoop();
void publishToMQTT(const char* message);

#endif
