#include "mqtt_samw.h"
#include "MQTTClient/Wrapper/mqtt.h"
#include "driver/include/m2m_wifi.h"
#include "connect_wifi.h"
#include "commands.h"
#include "eeprom_emul.h"
#include "timer_samw.h"

#include <usart.h>
#include <usart_interrupt.h>
#include <string.h>

extern uint8_t wifi_ssid[30];
extern uint8_t wifi_pass[30];

/*
 * A MQTT broker server which was connected.
 * m2m.eclipse.org is public MQTT broker.
 */

static const char main_mqtt_broker[] = "mqtt.eclipse.org";									//adres brokera MQTT - do zmiany na AWS

/** User name of chat. */
char mqtt_user[64] = "SAMW";

/** Chat topic. */
//char topic[64];

/* Instance of MQTT service. */
struct mqtt_module mqtt_inst;

extern struct usart_module cdc_uart_module;

/* Receive buffer of the MQTT service. */
static unsigned char mqtt_read_buffer[MAIN_MQTT_BUFFER_SIZE];
static unsigned char mqtt_send_buffer[500];


static void wifi_callback(uint8 msg_type, void *msg_data)
{
	tstrM2mWifiStateChanged *msg_wifi_state;
	uint8 *msg_ip_addr;

	switch (msg_type) {
		case M2M_WIFI_RESP_CON_STATE_CHANGED:
		msg_wifi_state = (tstrM2mWifiStateChanged *)msg_data;
		if (msg_wifi_state->u8CurrState == M2M_WIFI_CONNECTED) {
			/* If Wi-Fi is connected. */
			printf("Wi-Fi connected\r\n");
			m2m_wifi_request_dhcp_client();
			} else if (msg_wifi_state->u8CurrState == M2M_WIFI_DISCONNECTED) {
			/* If Wi-Fi is disconnected. */
			printf("WiFi init failed, check SSID and password\r\n");
			
			/* Disconnect from MQTT broker. */
			/* Force close the MQTT connection, because cannot send a disconnect message to the broker when network is broken. */
			socketDeinit();
			mqtt_disconnect(&mqtt_inst, 1);
			mqtt_flag = false;
			m2m_wifi_deinit(NULL);
		}

		break;

		case M2M_WIFI_REQ_DHCP_CONF:
		msg_ip_addr = (uint8 *)msg_data;
		printf("Wi-Fi IP is %u.%u.%u.%u\r\n",
		msg_ip_addr[0], msg_ip_addr[1], msg_ip_addr[2], msg_ip_addr[3]);
		/* Try to connect to MQTT broker when Wi-Fi was connected. */
		mqtt_connect(&mqtt_inst, main_mqtt_broker);
		break;

		default:
		break;
	}
}


/**
 * \brief Callback to get the Socket event.
 *
 * \param[in] Socket descriptor.
 * \param[in] msg_type type of Socket notification. Possible types are:
 *  - [SOCKET_MSG_CONNECT](@ref SOCKET_MSG_CONNECT)
 *  - [SOCKET_MSG_BIND](@ref SOCKET_MSG_BIND)
 *  - [SOCKET_MSG_LISTEN](@ref SOCKET_MSG_LISTEN)
 *  - [SOCKET_MSG_ACCEPT](@ref SOCKET_MSG_ACCEPT)
 *  - [SOCKET_MSG_RECV](@ref SOCKET_MSG_RECV)
 *  - [SOCKET_MSG_SEND](@ref SOCKET_MSG_SEND)
 *  - [SOCKET_MSG_SENDTO](@ref SOCKET_MSG_SENDTO)
 *  - [SOCKET_MSG_RECVFROM](@ref SOCKET_MSG_RECVFROM)
 * \param[in] msg_data A structure contains notification informations.
 */
static void mqtt_socket_cb(SOCKET sock, uint8_t msg_type, void *msg_data)
{
	mqtt_socket_event_handler(sock, msg_type, msg_data);
}

/**
 * \brief Callback of gethostbyname function.
 *
 * \param[in] doamin_name Domain name.
 * \param[in] server_ip IP of server.
 */
static void mqtt_resolve_cb(uint8_t *doamin_name, uint32_t server_ip)
{
	mqtt_socket_resolve_handler(doamin_name, server_ip);
}

void SubscribeHandler(MessageData *msgData)
{
	/* You received publish message which you had subscribed. */
	/* Print Topic and message */
	//printf("\r\n %.*s",msgData->topicName->lenstring.len,msgData->topicName->lenstring.data);
	//printf(" >> ");
	printf("%.*s \r\n",(int)msgData->message->payloadlen,(char *)msgData->message->payload);
	
	
	
	//odczyt numerów kart z serwera przez mqtt, warunki rozpoczêcia listy kart do zmiany - czekanie na nawias kwadratowy mo¿e zapêtliæ program
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////	
	//odczyt_numerow((uint8 *)msgData->message->payload);
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
}

/**
 * \brief Callback to get the MQTT status update.
 *
 * \param[in] conn_id instance id of connection which is being used.
 * \param[in] type type of MQTT notification. Possible types are:
 *  - [MQTT_CALLBACK_SOCK_CONNECTED](@ref MQTT_CALLBACK_SOCK_CONNECTED)
 *  - [MQTT_CALLBACK_CONNECTED](@ref MQTT_CALLBACK_CONNECTED)
 *  - [MQTT_CALLBACK_PUBLISHED](@ref MQTT_CALLBACK_PUBLISHED)
 *  - [MQTT_CALLBACK_SUBSCRIBED](@ref MQTT_CALLBACK_SUBSCRIBED)
 *  - [MQTT_CALLBACK_UNSUBSCRIBED](@ref MQTT_CALLBACK_UNSUBSCRIBED)
 *  - [MQTT_CALLBACK_DISCONNECTED](@ref MQTT_CALLBACK_DISCONNECTED)
 *  - [MQTT_CALLBACK_RECV_PUBLISH](@ref MQTT_CALLBACK_RECV_PUBLISH)
 * \param[in] data A structure contains notification informations. @ref mqtt_data
 */
static void mqtt_callback(struct mqtt_module *module_inst, int type, union mqtt_data *data)
{
	switch (type) {
	case MQTT_CALLBACK_SOCK_CONNECTED:
	{
		/*
		 * If connecting to broker server is complete successfully, Start sending CONNECT message of MQTT.
		 * Or else retry to connect to broker server.
		 */
		if (data->sock_connected.result >= 0) {
			printf("\r\nConnecting to Broker...");
			mqtt_connect_broker(module_inst, 1, NULL, NULL, mqtt_user, NULL, NULL, 0, 0, 0);
		} else {
			printf("Connect fail to server(%s)! retry it automatically.\r\n", main_mqtt_broker);
			mqtt_connect(module_inst, main_mqtt_broker); /* Retry that. */
		}
	}
	break;

	case MQTT_CALLBACK_CONNECTED:
		if (data->connected.result == MQTT_CONN_RESULT_ACCEPT) {
			/* Subscribe chat topic. */
			mqtt_subscribe(module_inst, (const char *)MAIN_CHAT_TOPIC, 1, SubscribeHandler);
			printf("Preparation of the chat has been completed.\r\n");
		} else {
			/* Cannot connect for some reason. */
			printf("MQTT broker decline your access! error code %d\r\n", data->connected.result);
		}

		break;

	case MQTT_CALLBACK_DISCONNECTED:
		/* Stop timer and USART callback. */
		printf("MQTT disconnected\r\n");
		break;
	}
}


/**
 * \brief Configure MQTT service.
 */
void configure_mqtt(void)
{
	struct mqtt_config mqtt_conf;
	int result;
	mqtt_get_config_defaults(&mqtt_conf);
	/* To use the MQTT service, it is necessary to always set the buffer and the timer. */
	mqtt_conf.read_buffer = mqtt_read_buffer;
	mqtt_conf.read_buffer_size = MAIN_MQTT_BUFFER_SIZE;
	mqtt_conf.send_buffer = mqtt_send_buffer;
	mqtt_conf.send_buffer_size = MAIN_MQTT_BUFFER_SIZE;
	
	result = mqtt_init(&mqtt_inst, &mqtt_conf);
	if (result < 0) {
		printf("MQTT initialization failed. Error code is (%d)\r\n", result);
		while (1) {
		}
	}

	result = mqtt_register_callback(&mqtt_inst, mqtt_callback);
	if (result < 0) {
		printf("MQTT register callback failed. Error code is (%d)\r\n", result);
		while (1) {
		}
	}
}

//funkcja wywo³ywana w mainie, obs³uga zdarzeñ MQTT
void mqtt_main_handler(void)
{
	if(mqtt_inst.isConnected) mqtt_yield(&mqtt_inst, 0);
}

//wywo³ywane przez funkcjê obs³uguj¹c¹ komendê, uruchomienie wifi i timera SysTick, dalej w callbackach po³¹czenie przez MQTT
void mqtt_connect_samw(void)
{
	memset(&wifi_ssid, 0, 30);
	EEPROM_Read(SSID_ADDRESS, wifi_ssid);
	memset(&wifi_pass, 0, 30);
	EEPROM_Read(PASSWORD_ADDRESS, wifi_pass);
	tstrWifiInitParam param;
	int8_t ret;
	
	/* Initialize Wi-Fi parameters structure. */
	memset((uint8_t *)&param, 0, sizeof(tstrWifiInitParam));

	/* Initialize Wi-Fi driver with data and status callbacks. */
	param.pfAppWifiCb = wifi_callback; /* Set Wi-Fi event callback. */
	ret = m2m_wifi_init(&param);
	if (M2M_SUCCESS != ret) {
		printf("main: m2m_wifi_init call error!(%d)\r\n", ret);
		while (1) { /* Loop forever. */
		}
	}

	/* Initialize socket interface. */
	socketInit();
	registerSocketCallback((tpfAppSocketCb)mqtt_socket_cb, (tpfAppResolveCb)mqtt_resolve_cb);

	/* Connect to router. */
	m2m_wifi_connect((char *)wifi_ssid, sizeof(wifi_ssid), MAIN_WLAN_AUTH, (char *)wifi_pass, M2M_WIFI_CH_ALL);

	if (SysTick_Config(system_cpu_clock_get_hz() / 1000))
	{
		puts("ERR>> Systick configuration error\r\n");
		while (1);
	}
}

//wywo³ywana w funkcji obs³uguj¹cej komendê mqtt-close - zamkniêcie komunikacji MQTT i wy³¹czenie modu³u wifi
void mqtt_close_samw(void)
{
	mqtt_deinit(&mqtt_inst);
	socketDeinit();
	if (mqtt_inst.network.socket > -1) close(mqtt_inst.network.socket);
	m2m_wifi_deinit(NULL);
	mqtt_flag = false;
}