#ifndef MQTT_H
#define MQTT_H

#include <PubSubClient.h>

extern PubSubClient mqttClient;

void setupMQTT();
void connectToMQTT();
void publishToMQTT(const char* message);
void mqttLoop();

#endif
