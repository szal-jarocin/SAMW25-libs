#ifndef _CONN_WIFI_H_
#define _CONN_WIFI_H_

#include "socket/include/socket.h"
#include "main.h"
#include <stdbool.h>

/** Wi-Fi Settings */
//#define MAIN_WLAN_SSID					"TP-LINK_BA49F3" /**< Destination SSID */
#define MAIN_WLAN_AUTH                  M2M_WIFI_SEC_WPA_PSK /**< Security manner */
//#define MAIN_WLAN_PSK                   "79053204" /**< Password for Destination SSID */

/** All SSL defines */

#define MAIN_HOST_NAME                  "5fhb3p3ok1.execute-api.eu-central-1.amazonaws.com"
#define MAIN_HOST_PORT                  443

/** Using IP address. */
#define IPV4_BYTE(val, index)           ((val >> (index * 8)) & 0xFF)


#define MAIN_HTTP_PROV_SERVER_DOMAIN_NAME	"safliconfig.com"
#define MAIN_M2M_AP_SEC						M2M_WIFI_SEC_OPEN
#define MAIN_M2M_AP_WEP_KEY					"1234567890"
#define MAIN_M2M_AP_SSID_MODE				SSID_MODE_VISIBLE

#define MAIN_M2M_DEVICE_NAME                 "WifiReader_0"
#define MAIN_MAC_ADDRESS                     {0xf8, 0xf0, 0x05, 0x45, 0xD4, 0x84}


static CONST char gacHttpProvDomainName[] = MAIN_HTTP_PROV_SERVER_DOMAIN_NAME;


extern SOCKET tcp_client_socket;
extern bool gbConnectedWifi;
extern bool gbHostIpByName;
extern uint8_t gau8SocketTestBuffer[1460];
extern uint32_t gu32HostIp;
extern bool ssl_flag;
extern bool mqtt_flag;
extern bool i2c_flag;
extern uint8_t gu8SocketStatus;

void parse_cards_list(uint8_t *buffer_pointer);
void ssl_connect(uint8_t argc, uint8_t * argv[]);
void ssl_close_command(uint8_t argc, uint8_t * argv[]);
void ssl_main_handler(void);
void wifi_cb(uint8_t u8MsgType, void *pvMsg);
void wifi_main_handler(void);
void wps_connect(void);
void wifi_config_http(void);
void wifi_cancel_config(void);
void wifi_scan(void);

#endif	//_CONN_WIFI_H_