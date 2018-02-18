#pragma once

static char ssid[] = "NYCR24";
static char password[] = "clubmate";

// mqtts://test_username:12345@io.adafruit.com
static const int mqtt_port = 1883;
static const char mqtt_id[] = "motion";
static const char mqtt_topic[] = "motion";

#if 0
static const char mqtt_server[] = "io.adafruit.com";
#define MQTT_USER "hudson_trmm"
static const char mqtt_password[] = "c055da2b599e4dd9b85eb504f5c92d86";
#else
#define mqtt_server "10.0.1.43"
#define MQTT_USER "hass"
#define mqtt_password ""
#endif
