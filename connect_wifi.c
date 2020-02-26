#include <string.h>
#include <delay.h>
#include "driver/include/m2m_wifi.h"
#include "socket/include/socket.h"
#include "connect_wifi.h"
#include "eeprom_emul.h"
#include "commands.h"
#include "NTP_sync.h"
#include "log_export.h"
#include "interpreter.h"
#include "ASCII_definitions.h"

//zmienne z innych bibliotek
extern bool receivedTime;
extern bool WPS_flag;
extern bool http_config_flag;
extern bool w_error_flag;
extern bool scanning_flag;
extern void (*command_pointer)(uint8_t argc, uint8_t *argv[]);
extern volatile enum Console_Res_t uart_state;
extern struct usart_module cdc_uart_module;
extern volatile uint8_t tick30ms;

uint8_t wifi_ssid[SSID_LENGTH+1];
uint8_t wifi_pass[PASSWORD_LENGTH+1];
uint8_t serial_num[SERIAL_LENGTH+1];
volatile uint16_t response_char_count = 0;
volatile uint16_t response_char_max = 0;

//zmienne u¿ywane do konfiguracji sieci wifi przez http
static uint8 gau8MacAddr[] = MAIN_MAC_ADDRESS;
static sint8 gacDeviceName[30] = MAIN_M2M_DEVICE_NAME;

static tstrM2MAPConfig gstrM2MAPConfig = {
	MAIN_M2M_DEVICE_NAME, 1, 0, WEP_40_KEY_STRING_SIZE, MAIN_M2M_AP_WEP_KEY, (uint8)MAIN_M2M_AP_SEC, MAIN_M2M_AP_SSID_MODE
};

//nawi¹zanie po³¹czenia przez SSL
static int8_t sslConnect(void)
{
	struct sockaddr_in addr_in;

	addr_in.sin_family = AF_INET;
	addr_in.sin_port = _htons(MAIN_HOST_PORT);
	addr_in.sin_addr.s_addr = gu32HostIp;

	/* Create secure socket */
	if (tcp_client_socket < 0) {
		tcp_client_socket = socket(AF_INET, SOCK_STREAM, SOCKET_FLAGS_SSL);
	}

	/* Check if socket was created successfully */
	if (tcp_client_socket == -1) {
		close(tcp_client_socket);
		return -1;
	}

	/* If success, connect to socket */
	if (connect(tcp_client_socket, (struct sockaddr *)&addr_in, sizeof(struct sockaddr_in)) != SOCK_ERR_NO_ERROR) {
		return SOCK_ERR_INVALID;
	}

	/* Success */
	return SOCK_ERR_NO_ERROR;
}

//do komunikacji SSL - z biblioteki
void wifi_cb(uint8_t u8MsgType, void *pvMsg)
{
	static uint8	u8ScanResultIdx = 0;
	
	switch (u8MsgType) {
		case M2M_WIFI_RESP_CON_STATE_CHANGED:
		{
			tstrM2mWifiStateChanged *pstrWifiState = (tstrM2mWifiStateChanged *)pvMsg;
			if (pstrWifiState->u8CurrState == M2M_WIFI_CONNECTED) {
				//printf("wifi_cb: M2M_WIFI_RESP_CON_STATE_CHANGED: CONNECTED\r\n");
				m2m_wifi_request_dhcp_client();
				
				} else if (pstrWifiState->u8CurrState == M2M_WIFI_DISCONNECTED) {
				//printf("wifi_cb: M2M_WIFI_RESP_CON_STATE_CHANGED: DISCONNECTED\r\n");					//komunikat podawany przy b³êdnym haœle lub SSID
				
				if (ssl_flag)
				{
					uart_buffer_clear();
					ssl_flag = false;
					socketDeinit();
					gbConnectedWifi = false;
					gbHostIpByName = false;
					m2m_wifi_deinit(NULL);
					switch (pstrWifiState->u8ErrCode)
					{
					case 0:
						send_command_check("w-error 01");			//sieæ niepod³¹czona do internetu
						break;
					case M2M_ERR_SCAN_FAIL:
						send_command_check("w-error 02");			//brak sieci o podanej nazwie (lub chwilowy brak zasiêgu?)
						break;
					case M2M_ERR_AUTH_FAIL:
						send_command_check("w-error 03");			//b³êdne has³o
						break;
					default:
						send_command_check("w-error 04");			//inny b³¹d po³¹czenia
						break;
					}
				}
				
// 				uart_add_to_buffer("WiFi init failed, check SSID and password");
// 				send_data();
				
				//przerwanie próby po³¹czenia ¿eby nie zapêtliæ programu, mo¿liwoœæ wprowadzenia has³a i nazwy
			}
			break;
		}

		case M2M_WIFI_REQ_DHCP_CONF:
		{
			//uint8_t *pu8IPAddress = (uint8_t *)pvMsg;
			/* Turn LED0 on to declare that IP address received. */
			//printf("wifi_cb: M2M_WIFI_REQ_DHCP_CONF: IP is %u.%u.%u.%u\r\n",
			//pu8IPAddress[0], pu8IPAddress[1], pu8IPAddress[2], pu8IPAddress[3]);
			gbConnectedWifi = true;

			/* Obtain the IP Address by network name */
			if (ssl_flag) gethostbyname((uint8_t *)MAIN_HOST_NAME);					//adres hosta dla nawi¹zania po³¹czenia przez SSL
			else if (NTP_sync_flag) gethostbyname((uint8_t *)MAIN_WORLDWIDE_NTP_POOL_HOSTNAME);		//adres hosta - serwer NTP
			
			break;
		}
		
		case M2M_WIFI_RESP_GET_SYS_TIME:
		{
			//printf("Received time\r\n");
			receivedTime = true;
			break;
		}

		case M2M_WIFI_REQ_WPS:
		{
			tstrM2MWPSInfo *pstrWPS = (tstrM2MWPSInfo *)pvMsg;
			//printf("Wi-Fi request WPS\r\n");
			//printf("SSID : %s, Password : %s", pstrWPS->au8SSID, pstrWPS->au8PSK);
			if (pstrWPS->u8AuthType == 0) {
				//jakiœ return, wystawienie flagi
// 				printf("WPS is not enabled OR Timedout\r\n");
// 				m2m_wifi_request_scan(M2M_WIFI_CH_ALL);
				/* WPS is not enabled by firmware OR WPS monitor timeout.*/
				WPS_flag = false;
				m2m_wifi_deinit(NULL);
				uart_buffer_clear();
				send_command_check("w-error 05");				//b³¹d WPS - timeout lub nie uruchomiony na routerze
				
			} else if (WPS_flag)
			{
				EEPROM_Write(SSID_ADDRESS, (uint8_t *)pstrWPS->au8SSID, SSID_LENGTH);
				EEPROM_Write(PASSWORD_ADDRESS, (uint8_t *)pstrWPS->au8PSK, PASSWORD_LENGTH);
// 				printf("Request Wi-Fi connect\r\n");
				
				socketDeinit();
				gbConnectedWifi = false;
				gbHostIpByName = false;
				WPS_flag = false;
				m2m_wifi_deinit(NULL);
				uart_buffer_clear();
				send_command_check("wps-ready");
				
//				m2m_wifi_connect((char *)pstrWPS->au8SSID, (uint8)m2m_strlen(pstrWPS->au8SSID), pstrWPS->u8AuthType, pstrWPS->au8PSK, pstrWPS->u8Ch);
			}
			break;
		}
		
		case M2M_WIFI_RESP_PROVISION_INFO:
		{
			tstrM2MProvisionInfo *pstrProvInfo = (tstrM2MProvisionInfo *)pvMsg;
			//printf("wifi_cb: M2M_WIFI_RESP_PROVISION_INFO.\r\n");
			
			if (pstrProvInfo->u8Status == M2M_SUCCESS) {
				EEPROM_Write(SSID_ADDRESS, (uint8_t *)pstrProvInfo->au8SSID, SSID_LENGTH);
				EEPROM_Write(PASSWORD_ADDRESS, (uint8_t *)pstrProvInfo->au8Password, PASSWORD_LENGTH);
				
 				socketDeinit();
 				gbConnectedWifi = false;
 				http_config_flag = false;
 				m2m_wifi_stop_provision_mode();
 				m2m_wifi_deinit(NULL);
				send_command_check("conf-ready");
				
				//m2m_wifi_connect((char *)pstrProvInfo->au8SSID, strlen((char *)pstrProvInfo->au8SSID), pstrProvInfo->u8SecType, pstrProvInfo->au8Password, M2M_WIFI_CH_ALL);
			} else {
				gbConnectedWifi = false;
				http_config_flag = false;
				m2m_wifi_stop_provision_mode();
				delay_ms(3);
				m2m_wifi_deinit(NULL);
				send_command_check("w-error 06");					//b³¹d w-config - urz¹dzenie roz³¹czone bez zapisywania SSID i has³a
			}
			break;
		}
		
		case M2M_WIFI_RESP_SCAN_DONE:
		{
			tstrM2mScanDone	*pstrInfo = (tstrM2mScanDone*)pvMsg;
			if(pstrInfo->s8ScanState == M2M_SUCCESS)
			{
				u8ScanResultIdx = 0;
				if(pstrInfo->u8NumofCh >= 1)
				{
					m2m_wifi_req_scan_result(u8ScanResultIdx);		//pobierz dane pierwszej znalezionej sieci
					//u8ScanResultIdx ++;
				}
				else
				{
					usart_write_buffer_job(&cdc_uart_module, (uint8_t *)"No AP found", strlen("No AP found"));
					//printf("No AP found");
					
					//wy³¹cz wifi i usuñ flagê
					scanning_flag = false;
					m2m_wifi_deinit(NULL);
					
				}
			}
			else
			{
				printf("Error %d", pstrInfo->s8ScanState);
				
				//wy³¹cz wifi i usuñ flagê
				scanning_flag = false;
				m2m_wifi_deinit(NULL);
			}
			break;
		}
		
		case M2M_WIFI_RESP_SCAN_RESULT:
		{
			tstrM2mWifiscanResult		*pstrScanResult =(tstrM2mWifiscanResult*)pvMsg;
			uint8						u8NumFoundAPs = m2m_wifi_get_num_ap_found();
			
			memset(wifi_ssid, 0, SSID_LENGTH+1);
			EEPROM_Read(SSID_ADDRESS, wifi_ssid, SSID_LENGTH);
			
			
			
			for (int i=0; i<=32; i++)
			{
				if (pstrScanResult->au8SSID[i] != wifi_ssid[i]) break;
				else if (i==32 || wifi_ssid[i] == 0)
				{
					//nazwa siê zgadza, podaj si³ê sygna³u
					
					char string[10];
					sprintf(string, "w-rssi %d", (pstrScanResult->s8rssi * -1));
					send_command_check(string);
				
					//wy³¹cz wifi i usuñ flagê
					scanning_flag = false;
					m2m_wifi_deinit(NULL);
					break;
				}
			}
			
			if (u8ScanResultIdx < u8NumFoundAPs - 1)
			{
				// Read the next scan result
				u8ScanResultIdx++;
				m2m_wifi_req_scan_result(u8ScanResultIdx);
			}
			else
			{
				send_command_check("w-rssi x");			//brak sieci o podanym SSID
				//wy³¹cz wifi i usuñ flagê
				scanning_flag = false;
				m2m_wifi_deinit(NULL);
			}
			break;
		}
		
		default:
			break;
	}
}

//do SSL - z biblioteki
static void ssl_resolve_cb(uint8_t *hostName, uint32_t hostIp)
{
	gu32HostIp = hostIp;
	gbHostIpByName = true;
// 	printf("Host IP is %d.%d.%d.%d\r\n", (int)IPV4_BYTE(hostIp, 0), (int)IPV4_BYTE(hostIp, 1),
// 	(int)IPV4_BYTE(hostIp, 2), (int)IPV4_BYTE(hostIp, 3));
// 	printf("Host Name is %s\r\n", hostName);
// 	uart_buffer_clear();
// 	uart_add_to_buffer("OK");
// 	send_data();
}

//do SSL - z biblioteki
static void ssl_socket_cb(SOCKET sock, uint8_t u8Msg, void *pvMsg)
{
	switch (u8Msg) {
		/* Socket connected */
		case SOCKET_MSG_CONNECT:
		{
			tstrSocketConnectMsg *pstrConnect = (tstrSocketConnectMsg *)pvMsg;
			if (pstrConnect && pstrConnect->s8Error >= 0) {
// 				printf("socket_cb: connect success!\r\n");
				if (command_pointer == log_trigger_command) log_handler();
				if (command_pointer == list_download_command) list_download_command(0, 0);
			} else {
// 				printf("socket_cb: connect error!\r\n");
				close(tcp_client_socket);
				tcp_client_socket = -1;
			}
		}
		break;

		/* Message send */
		case SOCKET_MSG_SEND:
		{
// 			printf("socket_cb: send success!\r\n");
			recv(tcp_client_socket, gau8SocketTestBuffer, sizeof(gau8SocketTestBuffer), 0);
		}
		break;

		/* Message receive */
		case SOCKET_MSG_RECV:
		{
			tstrSocketRecvMsg *pstrRecv = (tstrSocketRecvMsg *)pvMsg;
			if (pstrRecv && pstrRecv->s16BufferSize > 0) {
// 				printf("%s", pstrRecv->pu8Buffer);
				
								
				if (command_pointer == log_end_command)
				{
					//sprawdzenie, czy wysy³ka siê powiod³a
					//9 element macierzy - pierwszy znak kodu
					uint16_t response = 0;
//					printf("%s", gau8SocketTestBuffer);
					response = ((gau8SocketTestBuffer[9] - 48) * 100 + (gau8SocketTestBuffer[10] - 48) * 10 + (gau8SocketTestBuffer[11] - 48));
					uart_buffer_clear();
					
					if (response == 200) send_command_check("log-a");
					else send_command_check("log-x");



//					send_data();
//					send_command_check("log-end");
// 					printf("\r\n--------------------------------------------------------------\r\n");
// 					printf("%s", pstrRecv->pu8Buffer);
// 					printf("\r\n--------------------------------------------------------------\r\n");
// 					
					
					
					//jeœli nie to ponowna próba
					
					//jeœli siê powiod³a
					ssl_close_command(0, 0);
				}
				if (command_pointer == list_download_command || command_pointer == list_get_command)
				{
					//odczyt paczki listy kart - nie wiadomo czy pierwszy, czy kolejny - sprawdzenie musi mieæ miejsce w funkcji parse_cards_list
//					uint16_t response = 0;
					
//					if (command_pointer == list_download_command) response = ((gau8SocketTestBuffer[9] - 48) * 100 + (gau8SocketTestBuffer[10] - 48) * 10 + (gau8SocketTestBuffer[11] - 48));
//					else response = 200;
					
					printf("%s", (const char *)pstrRecv->pu8Buffer);
					
//					if (response == 200)
//					{
	
						//ODCZYT LICZBY ZNAKÓW CA£EJ WIADOMOŒCI
						if (command_pointer == list_download_command)
						{
							//102 103 104 105
							response_char_count = 0;
							response_char_max = 0;
							
							while (gau8SocketTestBuffer[response_char_count] != 123)		//liczenie znaków nag³ówka a¿ do znaku 123 który poprzedza liczbê znaków ca³ej wiadomoœci
							{
								response_char_count++;
							}
							
							uint8_t cr_index = 0;
							while (gau8SocketTestBuffer[102+cr_index] != 13)			//po znalezieniu znaku 123 szukanie znaku CR, w zale¿noœci od jego po³o¿enia liczba znaków ca³ej listy sk³ada siê z 1, 2, 3... cyfr
							{
								cr_index++;
							}
							
							switch (cr_index)
							{
							case 1:
								response_char_max = response_char_count + (gau8SocketTestBuffer[102]-48);
								break;
							case 2:
								response_char_max = response_char_count + ((gau8SocketTestBuffer[102]-48)*10 + (gau8SocketTestBuffer[103]-48));
								break;
							case 3:
								response_char_max = response_char_count + ((gau8SocketTestBuffer[102]-48)*100 + (gau8SocketTestBuffer[103]-48)*10 + (gau8SocketTestBuffer[104]-48));
								break;
							case 4:
								response_char_max = response_char_count + ((gau8SocketTestBuffer[102]-48)*1000 + (gau8SocketTestBuffer[103]-48)*100 + (gau8SocketTestBuffer[104]-48)*10 + (gau8SocketTestBuffer[105]-48));
								break;
							case 5:
								if (gau8SocketTestBuffer[102] <= 54 && gau8SocketTestBuffer[103] <= 53 && gau8SocketTestBuffer[104] <= 53)
								{
									response_char_max = response_char_count + ((gau8SocketTestBuffer[102]-48)*10000 + (gau8SocketTestBuffer[103]-48)*1000 + (gau8SocketTestBuffer[104]-48)*100 + (gau8SocketTestBuffer[105]-48)*10 + (gau8SocketTestBuffer[106]-48));
								}
								break;
							default:
								break;
							}
							response_char_count = 0;
							
						}
						response_char_count += pstrRecv->s16BufferSize;
						
						
						
						
						//parse_cards_list((uint8 *)pstrRecv->pu8Buffer);
						
						
						
						
						if (response_char_count < response_char_max)
						{
							
							//je¿eli nie zosta³y odczytane wszystkie znaki
							memset(gau8SocketTestBuffer, 0, sizeof(gau8SocketTestBuffer));
							recv(tcp_client_socket, gau8SocketTestBuffer, sizeof(gau8SocketTestBuffer), 0);		//odczyt nastêpnej paczki
						}
						else
						{
							//jeœli wszystkie znaki zosta³y odczytane
							//dodaæ warunek sprawdzania znaków "]>"
							memset(gau8SocketTestBuffer, 0, sizeof(gau8SocketTestBuffer));
							
							ssl_close_command(0, 0);

							// 	 					printf("%c\r\n > %c", ETX,  DC4);
						}
//  					}
//  					else if (response == 502)
//  					{
//  						//prawdopodobnie b³êdny serial
//  						send_command_check("w-error 08");
//  						ssl_close_command(0, 0);
//  					}
//  					else
//  					{
//  						//inny b³¹d wifi
//  						send_command_check("w-error 04");
//  						ssl_close_command(0, 0);
//  					}
				}
				
			}
			else tcp_client_socket = -1;
		}
		break;

		default:
		break;
	}
}

//po³¹czenie z AWS poprzez SSL
void ssl_connect(uint8_t argc, uint8_t * argv[])
{
// 	uint8_t wifi_ssid[SSID_LENGTH];				//tablica, do której pobierane s¹ znaki nazwy wifi z EEPROM
// 	uint8_t wifi_pass[PASSWORD_LENGTH];				//tablica znaków has³a z EEPROM
	memset(&wifi_ssid, 0, SSID_LENGTH+1);										//pobranie nazwy i has³a wifi z EEPROM
	EEPROM_Read(SSID_ADDRESS, wifi_ssid, SSID_LENGTH);
	memset(&wifi_pass, 0, PASSWORD_LENGTH+1);
	EEPROM_Read(PASSWORD_ADDRESS, wifi_pass, PASSWORD_LENGTH);
	
	if (wifi_ssid[0] == 255 || wifi_pass[0] == 255) return;
	
	tstrWifiInitParam param;
	int8_t ret;
	
	/* Initialize Wi-Fi parameters structure. */
	memset((uint8_t *)&param, 0, sizeof(tstrWifiInitParam));

	/* Initialize Wi-Fi driver with data and status callbacks. */
	param.pfAppWifiCb = wifi_cb;
	ret = m2m_wifi_init(&param);
	if (M2M_SUCCESS != ret) {
// 		printf("main: m2m_wifi_init call error!(%d)\r\n", ret);
		while (1) {
		}
	}

	/* Initialize Socket module */
	socketInit();
	if (ssl_flag) registerSocketCallback((tpfAppSocketCb)ssl_socket_cb, (tpfAppResolveCb)ssl_resolve_cb);		//callbacki dla po³¹czenia z AWS
	else if (NTP_sync_flag)	NTP_registerSocketCallback();														//callbacki dla synchronizacji czasu z NTP

	/* Connect to router. */
	m2m_wifi_connect((char *)wifi_ssid, strlen((char *)wifi_ssid), M2M_WIFI_SEC_WPA_PSK, (char *)wifi_pass, M2M_WIFI_CH_ALL);
}

//zakoñczenie komunikacji i wy³¹czenie modu³u wi-fi
void ssl_close_command(uint8_t argc, uint8_t * argv[])
{
	if (ssl_flag)
	{
		close(tcp_client_socket);
		socketDeinit();
		m2m_wifi_deinit(NULL);
		gu8SocketStatus = 0;
		ssl_flag = false;							//zerowanie flagi po³¹czenia SSL
		gbConnectedWifi = false;
		gbHostIpByName = false;
		tcp_client_socket = -1;
	}
	else
	{
		uart_buffer_clear();
		uart_add_to_buffer("SSL not connected");
		send_data();
	}
}

void ssl_main_handler(void)
{
	m2m_wifi_handle_events(NULL);
	if (gbConnectedWifi && gbHostIpByName) {
		if (gu8SocketStatus == SocketInit) {
			if (tcp_client_socket < 0) {
				gu8SocketStatus = SocketWaiting;
				if (sslConnect() != SOCK_ERR_NO_ERROR) {
					gu8SocketStatus = SocketInit;
					delay_s(1);
				}
			}
		}
	}
}

void wps_connect(void)
{
	tstrWifiInitParam param;
	int8_t ret;
	char devName[] = "WifiReader_WPS";
	/* Initialize Wi-Fi parameters structure. */
	memset((uint8_t *)&param, 0, sizeof(tstrWifiInitParam));
	param.pfAppWifiCb = wifi_cb;
	/* Initialize WINC1500 Wi-Fi driver with data and status callbacks. */
	ret = m2m_wifi_init(&param);
	if (M2M_SUCCESS != ret) {
		
		//przenieœæ do maina?
		
		
	}
	else
	{
		m2m_wifi_set_device_name((uint8 *)devName, strlen(devName));
		m2m_wifi_wps(WPS_PBC_TRIGGER, NULL);
	}
}

//obs³uga zdarzeñ wifi w funkcji main
void wifi_main_handler(void)
{
	m2m_wifi_handle_events(NULL);
}

/*#define HEX2ASCII(x) (((x) >= 10) ? (((x) - 10) + 'A') : ((x) + '0'))*/
// static void set_dev_name_to_mac(uint8 *name, uint8 *mac_addr)
// {
// 	/* Name must be in the format WINC1500_00:00 */
// 	uint16 len;
// 
// 	len = m2m_strlen(name);
// 	if (len >= 5) {
// 		name[len - 1] = HEX2ASCII((mac_addr[5] >> 0) & 0x0f);
// 		name[len - 2] = HEX2ASCII((mac_addr[5] >> 4) & 0x0f);
// 		name[len - 4] = HEX2ASCII((mac_addr[4] >> 0) & 0x0f);
// 		name[len - 5] = HEX2ASCII((mac_addr[4] >> 4) & 0x0f);
// 	}
// }


//konfiguracja urz¹dzenia przez stronê http
void wifi_config_http(void)
{
	tstrWifiInitParam param;
	int8_t ret;

	uint8_t mac_addr[6];
	uint8_t u8IsMacAddrValid;
	
	memset((uint8_t *)&param, 0, sizeof(tstrWifiInitParam));
	param.pfAppWifiCb = wifi_cb;
	ret = m2m_wifi_init(&param);
	
	if (M2M_SUCCESS != ret)
	{
		http_config_flag = false;
		w_error_flag = true;
		return;
	}
	
	m2m_wifi_get_otp_mac_address(mac_addr, &u8IsMacAddrValid);
	if (!u8IsMacAddrValid) {
		m2m_wifi_set_mac_address(gau8MacAddr);
	}

	m2m_wifi_get_mac_address(gau8MacAddr);
	
	memset(serial_num, 0, SERIAL_LENGTH+1);
	EEPROM_Read(SERIAL_ADDRESS, serial_num, SERIAL_LENGTH);
	sprintf((char *)gacDeviceName + strlen("WifiReader_"), "%s", (char *)serial_num);
	sprintf((char *)gstrM2MAPConfig.au8SSID, "%s", gacDeviceName);
	//set_dev_name_to_mac((uint8_t *)gacDeviceName, gau8MacAddr);
	//set_dev_name_to_mac((uint8_t *)gstrM2MAPConfig.au8SSID, gau8MacAddr);
	m2m_wifi_set_device_name((uint8_t *)gacDeviceName, (uint8_t)m2m_strlen((uint8_t *)gacDeviceName));
	gstrM2MAPConfig.au8DHCPServerIP[0] = 0xC0; /* 192 */
	gstrM2MAPConfig.au8DHCPServerIP[1] = 0xA8; /* 168 */
	gstrM2MAPConfig.au8DHCPServerIP[2] = 0x01; /* 1 */
	gstrM2MAPConfig.au8DHCPServerIP[3] = 0x01; /* 1 */
	
	m2m_wifi_start_provision_mode((tstrM2MAPConfig *)&gstrM2MAPConfig, (char *)gacHttpProvDomainName, 1);
}

//przerwanie konfiguracji przez stronê
void wifi_cancel_config(void)
{
	http_config_flag = false;
	m2m_wifi_stop_provision_mode();		//zatrzymanie trybu konfiguracji, wymaga delaya przed wy³¹czeniem atwinca
	delay_ms(3);
	m2m_wifi_deinit(NULL);				//wy³¹czenie modu³u wifi
}

//uruchomienie skanowania sieci w celu okreœlenia mocy sygna³u zapisanej sieci
void wifi_scan(void)
{
	tstrWifiInitParam 	param;
	
	param.pfAppWifiCb	= wifi_cb;
	if(!m2m_wifi_init(&param))
	{
		//jeœli poprawna inicjalizacja ATWINa
		m2m_wifi_request_scan(M2M_WIFI_CH_ALL);		//skanuj wszystkie kana³y
	}
}