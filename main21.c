/**
 *
 * \file
 *
 * \brief WINC1500 TCP Client Example.
 *
 * Copyright (c) 2016-2018 Microchip Technology Inc. and its subsidiaries.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Subject to your compliance with these terms, you may use Microchip
 * software and any derivatives exclusively with Microchip products.
 * It is your responsibility to comply with third party license terms applicable
 * to your use of third party software (including open source software) that
 * may accompany Microchip software.
 *
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE,
 * INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY,
 * AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT WILL MICROCHIP BE
 * LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, INCIDENTAL OR CONSEQUENTIAL
 * LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND WHATSOEVER RELATED TO THE
 * SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS BEEN ADVISED OF THE
 * POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.  TO THE FULLEST EXTENT
 * ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY
 * RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
 * THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *
 * \asf_license_stop
 *
 */

/** \mainpage
 * \section intro Introduction
 * This example demonstrates the use of the WINC1500 with the SAMD21 Xplained Pro
 * board to test TCP client.<br>
 * It uses the following hardware:
 * - the SAMD21 Xplained Pro.
 * - the WINC1500 on EXT1.
 *
 * \section files Main Files
 * - main.c : Initialize the WINC1500 and test TCP client.
 *
 * \section usage Usage
 * -# Configure below code in the main.h for AP information to be connected.
 * \code
 *    #define MAIN_WLAN_SSID                    "DEMO_AP"
 *    #define MAIN_WLAN_AUTH                    M2M_WIFI_SEC_WPA_PSK
 *    #define MAIN_WLAN_PSK                     "12345678"
 *    #define MAIN_WIFI_M2M_PRODUCT_NAME        "NMCTemp"
 *    #define MAIN_WIFI_M2M_SERVER_IP           0xFFFFFFFF // "255.255.255.255"
 *    #define MAIN_WIFI_M2M_SERVER_PORT         (6666)
 *    #define MAIN_WIFI_M2M_REPORT_INTERVAL     (1000)
 * \endcode
 * -# Build the program and download it into the board.
 * -# On the computer, open and configure a terminal application as the follows.
 * \code
 *    Baud Rate : 115200
 *    Data : 8bit
 *    Parity bit : none
 *    Stop bit : 1bit
 *    Flow control : none
 * \endcode
 * -# Start the application.
 * -# In the terminal window, the following text should appear:
 * \code
 *    -- WINC1500 TCP client example --
 *    -- SAMD21_XPLAINED_PRO --
 *    -- Compiled: xxx xx xxxx xx:xx:xx --
 *    wifi_cb: M2M_WIFI_RESP_CON_STATE_CHANGED: CONNECTED
 *    m2m_wifi_state: M2M_WIFI_REQ_DHCP_CONF: IP is xxx.xxx.xxx.xxx
 *    socket_cb: connect success!
 *    socket_cb: send success!
 *    socket_cb: recv success!
 *    TCP Client Test Complete!
 * \endcode
 *
 * \section compinfo Compilation Information
 * This software was written for the GNU GCC compiler using Atmel Studio 6.2
 * Other compilers may or may not work.
 *
 * \section contactinfo Contact Information
 * For further information, visit
 * <A href="http://www.atmel.com">Atmel</A>.\n
 */

#include <usart.h>
#include <usart_interrupt.h>

#include "asf.h"
#include "main.h"
#include "driver/include/m2m_wifi.h"
#include "socket/include/socket.h"
#include "driver/include/m2m_ssl.h"
#include "driver/include/ecc_types.h"
#include "common/include/nm_common.h"

#include "ASCII_definitions.h"
#include "connect_wifi.h"
#include "eeprom_emul.h"
#include "timer_samw.h"
#include "NTP_sync.h"
#include "aws_mqtt.h"
#include "interpreter.h"
#include "atwinc_flash.h"
#include "SPI_samd.h"

extern void i2c_main_handler(void);
extern volatile uint8_t tick30ms;
extern char time_buf[30];
extern uint8_t wifi_ssid[SSID_LENGTH+1];
extern uint8_t wifi_pass[PASSWORD_LENGTH+1];
extern uint8_t serial_num[SERIAL_LENGTH+1];


#define STRING_EOL    "\r\n"
#define STRING_HEADER "-- SSL example --"STRING_EOL \
	"-- "BOARD_NAME " --"STRING_EOL	\
	"-- Compiled: "__DATE__ " "__TIME__ " --"STRING_EOL

/** IP address of host. */
uint32_t gu32HostIp = 0;

uint8_t gu8SocketStatus = SocketInit;
uint8_t gau8SocketTestBuffer[1460];

/** TCP client socket handler. */
SOCKET tcp_client_socket = -1;

bool gbConnectedWifi = false;
bool gbHostIpByName = false;

//flagi zabezpieczaj¹ce przed podwójnym u¿yciem komendy
bool ssl_flag = false;
bool mqtt_flag = false;
bool mqtt_close_flag = false;
bool i2c_flag = false;
bool uart_flag = false;
bool mqtt_read_flag = false;
bool NTP_sync_flag = false;
bool mqtt_connected_flag = false;
bool list_start_flag = false;
bool list_mqtt_flag = false;
bool WPS_flag = false;
bool http_config_flag = false;
bool w_error_flag = false;
bool scanning_flag = false;
bool cert_write_flag = false;

uint16_t licznik = 0;
uint8_t znak = 48;

extern bool echo_loop_flag;

/**
 * \brief Main application function.
 *
 * Application entry point.
 *
 * \return program return value.
 */
int main(void)
{
	/* Initialize the board. */
	system_init();
	
	/* Initialize the UART console. */
	configure_console();
	//printf(STRING_HEADER);
	
	//konfiguracja timera u¿ywanego do odbierania znaków przez uart - w innym przypadku MQTT przerywa³o pracê uartu
	timer_init();
	
	/* Initialize the BSP. */
	nm_bsp_init();
		
	// konfiguracja EEPROM
	EEPROM_emul_config();
	
	// zapisanie wartoœci domyœlnych: SSID, has³a wi-fi i numeru seryjnego od eepromu
	EEPROM_save_defaults();
	
	
	
// 	#if C_USB
// 		udc_start();
// 	#endif
	
	#if C_RC522 || C_FLASH
		SPI_init_samd();
	#endif
	
	
/*	switch (m2m_ssl_set_active_ciphersuites(SSL_CIPHER_ECDHE_RSA_WITH_AES_128_GCM_SHA256))
	{
		case SOCK_ERR_NO_ERROR:
		printf("SOCK_ERR_NO_ERROR");
		break;
		
		case SOCK_ERR_INVALID_ARG:
		printf("SOCK_ERR_INVALID_ARG");
		break;
		
	}
	*/
	
	//przed wejœciem do while - sprawdzenie, czy czytnik zosta³ skonfigurowany
	//jeœli tak - na pierwszych pozycjach tablicy has³a i SSID znaki inne ni¿ 0xFF
	if (wifi_pass[0] != 255 && wifi_ssid[0] != 255) send_command_check("w-ready a");		//potwierdzenie - czytnik skonfigurowany
	else send_command_check("w-ready x");													//komenda informuj¹ca o braku konfiguracji
	
	while (1) {		
		
		//obs³uga wystawianych flag w celu cyklicznego wykonywania funkcji bez wykorzystywania timerów
		
		//obs³uga SSL
		if (ssl_flag)
		{
			if (wifi_pass[0] != 255 && wifi_ssid[0] != 255) ssl_main_handler();				//sprawdzenie konfiguracji czytnika
			else																			//jeœli nie zosta³ skonfigurowany - wyœlij kod b³êdu
			{
				ssl_flag = false;
				send_command_check("w-error 00");			//brak SSID i has³a w pamiêci
			}
		}
		
		//obs³uga œci¹gniêcia czasu z serwera NTP
		if (NTP_sync_flag) NTP_main_handler();
		
 		//obs³uga MQTT do po³¹czenia z AWS
 		if (mqtt_flag && !uart_flag)								//warunki do sprawdzenia - czy procesor jest w trakcie przerabiania znaku z uarta i czy zosta³o ju¿ nawi¹zane po³¹czenie przez MQTT
 		{
			 if (wifi_ssid[0] == 255 || wifi_pass[0] == 255)		//sprawdzenie konfiguracji czytnika
			 {
				 mqtt_close_flag = true;
				 send_command_check("w-error 00");					//brak SSID i has³a w pamiêci
			 }
			else aws_main_handler();								//obs³uga po³¹czenia przez MQTT
			
			if (mqtt_close_flag)									//jeœli pojawi³a siê flaga zamkniêcia po³¹czenia
			{
				aws_mqtt_deinit();									//zakoñczenie po³¹czenia MQTT i usuniêcie flag
				mqtt_close_flag = false;
				mqtt_connected_flag = false;
			}
 		}
		
		if (echo_loop_flag)											//sprawdzenie flagi pêtli echo (pêtla wysy³aj¹ca komendê echo i argument 000 111 itd)
		{
			licznik++;
			if (licznik >= 5000)
			{
				licznik = 0;
				if (znak == 57) znak = 48;
				else znak++;
				char bufor[8];
				sprintf(bufor, "echo %c%c%c", znak, znak, znak);
//				sprintf(bufor, "echo %u", tick30ms);
				send_command_check(bufor);
			}
		}
		
		//jeœli wystawiona któraœ z flag wymagaj¹cych uruchomienia wi-fi (konfiguracja WPS, sprawdzenie zasiêgu lub po³¹czenie)
		if (WPS_flag || http_config_flag || scanning_flag) wifi_main_handler();
		
		//jeœli flaga b³êdu przy inicjacji ATWINa (w mainie ¿eby dzia³a³ timer przy ponownym wys³aniu polecenia bez otrzymania odpowiedzi)
		if (w_error_flag)
		{
			send_command_check("w-error 07");				//b³¹d z w_error_flag - b³¹d przy inicjacji ATWINa, prawdopodobnie b³¹d SPI
			w_error_flag = false;
		}
		
		//funkcja zapisu certyfikatu urz¹dzenia do pamiêci atwina
		//wykonywana w mainie ¿eby dzia³a³o przerwanie od timera i odbieranie znaków z uarta
		if (cert_write_flag)
		{
			cert_write();
			cert_write_flag = false;
		}
	}

	return 0;
}