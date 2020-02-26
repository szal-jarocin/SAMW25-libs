#ifndef _MQTT_SAMW_H_
#define _MQTT_SAMW_H_

#include <stdint.h>
#include "MQTTClient/MQTTClient.h"

/* Max size of UART buffer. */
#define MAIN_CHAT_BUFFER_SIZE 500

/* Max size of MQTT buffer. */
#define MAIN_MQTT_BUFFER_SIZE 1460

/* Limitation of user name. */
#define MAIN_CHAT_USER_NAME_SIZE 64

/* Chat MQTT topic. */
//#define MAIN_CHAT_TOPIC		"$aws/things/Test_Tenant_26/shadow/get/accepted"
#define MAIN_CHAT_TOPIC "/zaq1@WSXZAQ!2wsx/mosquittotest/"		//testowy

//#define MAIN_WLAN_AUTH                  M2M_WIFI_SEC_WPA_PSK


/** Prototype for MQTT subscribe Callback */
void SubscribeHandler(MessageData *msgData);
void configure_mqtt(void);
void mqtt_main_handler(void);
void mqtt_connect_samw(void);
void mqtt_close_samw(void);
#endif /* _MQTT_SAMW_H_ */