#ifndef _AWS_MQTT_H_
#define _AWS_MQTT_H_


/*Role of the device*/
#define SUBSCRIBER
//#define PUBLISHER

#ifdef SUBSCRIBER
#define CLIENT_ID			"WINC1500_Sub"
//#define SUBSCRIBE_CHANNEL	"WINC1500_IOT/sub"
//#define PUBLISH_CHANNEL	"WINC1500_IOT/pub"
#define SUBSCRIBE_CHANNEL	"$aws/things/HALKO/state"
#define PUBLISH_CHANNEL		"$aws/things/HALKO/shadow/get"
//#define SUBSCRIBE_CHANNEL	"$aws/things/00108DEE/state"
//#define PUBLISH_CHANNEL		"$aws/things/00108DEE/shadow/get"
#else
#define CLIENT_ID "WINC1500_Pub"
#define SUBSCRIBE_CHANNEL "WINC1500_IOT/pub"
#define PUBLISH_CHANNEL   "WINC1500_IOT/sub"
#endif


void aws_mqtt_init(void);
void aws_mqtt_deinit(void);
void aws_main_handler(void);

#endif /* _AWS_MQTT_H_ */