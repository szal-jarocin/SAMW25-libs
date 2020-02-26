#include "NTP_sync.h"
#include "driver/include/m2m_wifi.h"
#include "socket/include/socket.h"
#include "interpreter.h"
#include "ASCII_definitions.h"
#include <string.h>
#include <time.h>

/** UDP socket handlers. */
static SOCKET udp_socket = -1;

extern bool NTP_sync_flag;
extern bool gbConnectedWifi;

/**
 * \brief Callback to get the Data from socket.
 *
 * \param[in] sock socket handler.
 * \param[in] u8Msg Type of Socket notification.
 * \param[in] pvMsg A structure contains notification informations.
 */
static void NTP_socket_cb(SOCKET sock, uint8_t u8Msg, void *pvMsg)
{
	/* Check for socket event on socket. */
	int16_t ret;

	switch (u8Msg) {
	case SOCKET_MSG_BIND:
	{
		/* printf("socket_cb: socket_msg_bind!\r\n"); */
		tstrSocketBindMsg *pstrBind = (tstrSocketBindMsg *)pvMsg;
		if (pstrBind && pstrBind->status == 0) {
			static uint8_t gau8SocketBuffer[MAIN_WIFI_M2M_BUFFER_SIZE];
			ret = recvfrom(sock, gau8SocketBuffer, MAIN_WIFI_M2M_BUFFER_SIZE, 0);
			if (ret != SOCK_ERR_NO_ERROR) {
// 				printf("socket_cb: recv error!\r\n");
// 				printf("%c\r\n > %c", ETX,  DC4);
			}
		} else {
// 			printf("socket_cb: bind error!\r\n");
// 			printf("%c\r\n > %c", ETX,  DC4);
		}

		break;
	}

	case SOCKET_MSG_RECVFROM:
	{
		/* printf("socket_cb: socket_msg_recvfrom!\r\n"); */
		tstrSocketRecvMsg *pstrRx = (tstrSocketRecvMsg *)pvMsg;
		if (pstrRx->pu8Buffer && pstrRx->s16BufferSize) {
			uint8_t packetBuffer[48];
			memcpy(&packetBuffer, pstrRx->pu8Buffer, sizeof(packetBuffer));

			if ((packetBuffer[0] & 0x7) != 4) {                   /* expect only server response */
// 				printf("socket_cb: Expecting response from Server Only!\r\n");
// 				printf("%c\r\n > %c", ETX,  DC4);
				return;                    /* MODE is not server, abort */
			} else {
				uint32_t secsSince1900 = packetBuffer[40] << 24 |
						packetBuffer[41] << 16 |
						packetBuffer[42] << 8 |
						packetBuffer[43];

				/* Now convert NTP time into everyday time.
				 * Unix time starts on Jan 1 1970. In seconds, that's 2208988800.
				 * Subtract seventy years.
				 */
				const uint32_t seventyYears = 2208988800UL;
				time_t epoch = secsSince1900 - seventyYears;
				struct tm ts;
				char time_buf[24];
				
				memset(time_buf, 0, 24);
				//setenv("TZ", "CET-1CEST-2", 1);											//czas œrodkowoeuropejski UTC+1, z przejœciem na czas letni UTC+2
				//tzset();																	//ustawienie strefy czasowej zapisanej wy¿ej jako zmienna œrodowiskowa, niepotrzebne - wszystkie u¿ywaj¹ czasu ze strefy UTC
				ts = *localtime(&epoch);													//uwzglêdnienie strefy czasowej
				strftime(time_buf, sizeof(time_buf), "w-time %y %m %d %H %M %S", &ts);				//przerobienie czasu uniksowego na czytelny dla cz³owieka
				send_command_check(time_buf);													//przes³anie aktualnego czasu do hosta
				//program nie wychodzi z funkcji wys³ania polecenia
				
				//zakoñczenie komunikacji z serwerem NTP
				gbConnectedWifi = false;
				NTP_sync_flag = false;
				ret = close(sock);
				if (ret == SOCK_ERR_NO_ERROR) {
					socketDeinit();
					m2m_wifi_deinit(NULL);
					udp_socket = -1;
				}
			}
		}
	//printf("%c\r\n > %c", ETX, DC4);						//gotowoœæ do odbioru kolejnego polecenia
	}
	break;

	default:
		break;
	}
}

/**
 * \brief Callback to get the ServerIP from DNS lookup.
 *
 * \param[in] pu8DomainName Domain name.
 * \param[in] u32ServerIP Server IP.
 */
static void NTP_resolve_cb(uint8_t *pu8DomainName, uint32_t u32ServerIP)
{
	struct sockaddr_in addr;
	int8_t cDataBuf[48];
	int16_t ret;

	memset(cDataBuf, 0, sizeof(cDataBuf));
	cDataBuf[0] = '\x1b'; /* time query */

	//printf("resolve_cb: DomainName %s\r\n", pu8DomainName);

	if (udp_socket >= 0) {
		/* Set NTP server socket address structure. */
		addr.sin_family = AF_INET;
		addr.sin_port = _htons(MAIN_SERVER_PORT_FOR_UDP);
		addr.sin_addr.s_addr = u32ServerIP;

		/*Send an NTP time query to the NTP server*/
		ret = sendto(udp_socket, (int8_t *)&cDataBuf, sizeof(cDataBuf), 0, (struct sockaddr *)&addr, sizeof(addr));
		if (ret != M2M_SUCCESS) {
// 			printf("resolve_cb: failed to send  error!\r\n");
// 			printf("%c\r\n > %c", ETX, DC4);
			return;
		}
	}
}

void NTP_registerSocketCallback(void)
{
	registerSocketCallback((tpfAppSocketCb)NTP_socket_cb, (tpfAppResolveCb)NTP_resolve_cb);
}

void NTP_main_handler(void)
{
	m2m_wifi_handle_events(NULL);
	struct sockaddr_in addr_in;
	if (gbConnectedWifi) {
		/*
			* Create the socket for the first time.
			*/
		if (udp_socket < 0) {
			udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
			if (udp_socket < 0) {
// 				printf("main: UDP Client Socket Creation Failed.\r\n");
			}

			/* Initialize default socket address structure. */
			addr_in.sin_family = AF_INET;
			addr_in.sin_addr.s_addr = _htonl(MAIN_DEFAULT_ADDRESS);
			addr_in.sin_port = _htons(MAIN_DEFAULT_PORT);

			bind(udp_socket, (struct sockaddr *)&addr_in, sizeof(struct sockaddr_in));
		}
	}
}

