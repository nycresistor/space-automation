#pragma once

/*
 * WiFi configuration for the things at the NYC Resistor space
 */
#if 1
#define WIFI_SSID	"NYCR24"
#define WIFI_PASSWORD	"clubmate"
#define MQTT_SERVER	"automation.local"
#else
#define WIFI_SSID	""
#define WIFI_PASSWORD	""
#define MQTT_SERVER	"10.0.1.43"
#endif

#define MQTT_PORT	1883
#define MQTT_USER	""
#define MQTT_PASSWORD	""
