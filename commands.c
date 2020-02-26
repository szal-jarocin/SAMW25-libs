#include <delay.h>
#include <stdint.h>
#include "samw25_xplained_pro.h"
#include "socket/include/socket.h"
#include "driver/include/m2m_wifi.h"
#include "MQTTClient/Wrapper/mqtt.h"
#include "main.h"
#include "commands.h"
#include "port.h"
#include "connect_wifi.h"
#include "eeprom_emul.h"
#include "mqtt_samw.h"
#include "i2c_master_samw.h"
#include "interpreter.h"
#include "aws_mqtt.h"
#include "ASCII_definitions.h"
#include "atwinc_flash.h"

#if C_USB
	#include "udc.h"
#endif


#if C_FLASH
	#include "SPI_samd.h"
	#include "SPI_mem.h"
#endif

#if C_ATECC608
	#include "atecc608.h"
#endif


extern uint8_t wifi_ssid[SSID_LENGTH+1];
extern uint8_t wifi_pass[PASSWORD_LENGTH+1];
extern uint8_t serial_num[SERIAL_LENGTH+1];
extern void (*command_pointer)(uint8_t argc, uint8_t *argv[]);
extern bool list_start_flag;
extern bool list_mqtt_flag;
extern bool WPS_flag;
extern bool http_config_flag;
extern bool scanning_flag;
extern uint16_t crc_interpreter;
extern uint8_t log_uart_buf[40];
bool echo_loop_flag = false;
volatile bool list_get_flag = false;
extern bool cert_write_flag;


//funkcja testowa przesy³aj¹ca do konsoli wszystkie odebrane argumenty funkcji
/*void DemoCmd_ArgumentsShow(uint8_t argc, uint8_t * argv[]) {
	printf("\r\nShow all passed arguments\r\nargc = ");
	printf("%i", argc);
	for(uint8_t i=0; i<MAX_ARGS; i++) {
		printf("\r\n");
		printf("%i", i);
		printf("\r\n");
		printf("%i",(uint16_t)argv[i]);
		printf("\r\n");
		printf((const char *)argv[i]);
	}
}
*/

//odczyt numeru seryjnego z pamiêci EEPROM i wys³anie przez UART
void serial_read(uint8_t argc, uint8_t * argv[])
{
	memset(serial_num, 0, SERIAL_LENGTH+1);
	EEPROM_Read(SERIAL_ADDRESS, serial_num, SERIAL_LENGTH);
	uart_add_to_buffer((char *)serial_num);
	send_data();
}


//funkcja odczytuj¹ca lub zapisuj¹ca nr seryjny urz¹dzenia
void serial_command(uint8_t argc, uint8_t * argv[])
{
	if (argc == 2)																//z argumentem - zapis nowego numeru seryjnego
	{
		EEPROM_Write(SERIAL_ADDRESS, (uint8_t *)argv[1], SERIAL_LENGTH);
		uart_buffer_clear();
		uart_add_to_buffer("Serial set: ");
		serial_read(argc, argv);
	}
	else if (argc == 1)															//bez argumentu - odczyt aktualnego numeru seryjnego
	{
		uart_buffer_clear();
		uart_add_to_buffer("Current serial: ");
		serial_read(argc, argv);
	}
	else																		//w innym przypadku - wyswietlenie pomocy
	{
		uart_buffer_clear();
		uart_add_to_buffer("serial num[HEX]");
		send_data();
	}
}

//podstawowa funkcja zapalenia i zgaszenia diody LED - sprawdzanie pracy interpretera
void led_switch(uint8_t argc, uint8_t * argv[])
{	
	if (argc == 2)
	{
		switch (*argv[1])
		{
		case '1':
			port_pin_set_output_level(LED_0_PIN, LED_0_ACTIVE);
			uart_buffer_clear();
			uart_add_to_buffer("OK");
			break;
		case '0':
			port_pin_set_output_level(LED_0_PIN, LED_0_INACTIVE);
			uart_buffer_clear();
			uart_add_to_buffer("OK");
			break;
		default:
			uart_buffer_clear();
			uart_add_to_buffer("Bad argument");
			break;
		}
	}
	else
	{
		uart_buffer_clear();
		uart_add_to_buffer("wf-led [ 0 / 1 ]");
	}
	send_data();
}

//pobranie listy kart z AWS przez SSL
void list_download_command(uint8_t argc, uint8_t * argv[])
{
	if (!list_mqtt_flag)									//sprawdzenie, czy nie zosta³o ustawione pobieranie listy przez mqtt komend¹ list-mqtt
	{
		if(!ssl_flag && !mqtt_flag)							//sprawdzenie, czy nie zosta³o wczeœniej zainicjowane po³¹czeniem przez SSL lub MQTT
		{
			uart_buffer_clear();
			uart_add_to_buffer("OK");
			send_data();
			ssl_connect_command(argc, argv);				//inicjowanie po³¹czenia przez SSL przed pobraniem listy
	// 		printf("Wifi not connected");
	// 		printf("%c\r\n > %c", ETX,  DC4);
		}
	
		else if (ssl_flag)									//jeœli po³¹czenie SSL jest ju¿ nawi¹zane - pobierz listê
		{
			char get_cards[256];
			EEPROM_Read(SERIAL_ADDRESS, serial_num, SERIAL_LENGTH);			//pobranie numeru seryjnego urz¹dzenia
			if (serial_num[0] == 255)
			{
				return;										//brak numeru seryjnego - wyjœcie z funkcji, w mainie wys³anie b³êdu o braku konfiguracji czytnika i usuniêcie flagi
			}
			else
			{
				//wys³anie requesta do AWSa, MUSI ZAWIERAÆ X-API-KEY

				//zbudowanie polecenia GET za pomoc¹ sprintf
				
				//ustawienie flagi otrzymania pierwszej paczki na 0
				list_start_flag = false;
				send(tcp_client_socket, &get_cards, sizeof(get_cards), 0);
			}
		}
		
		else check_wifi_flags();
	}
	else
	{
		//obs³uga pobrania listy kart przez MQTT
		uart_buffer_clear();
		uart_add_to_buffer("OK");
		send_data();
		
		
		
		
		
		
		
		
		//dodatkowy warunek kiedy siê po³¹czyæ a kiedy ju¿ jest po³¹czony
		
		
		
		
		
		
		
		
		mqtt_connect_command(argc, argv);
	}
}

//przes³anie kolejnego UID karty z listy
void list_get_command(uint8_t argc, uint8_t * argv[])
{
	if (!list_start_flag)			//lista nie zosta³a pobrana lub ca³a lista zosta³a przes³ana - wyœlij X
	{
		uart_buffer_clear();
		uart_add_to_buffer("X");
		send_data();
	}
	else							//przes³anie kolejnego UID
	{
		list_get_flag = true;
	}
}

//pobranie nazwy wifi zapisanej w EEPROM i przes³anie przez UART
void wifi_ssid_read(uint8_t argc, uint8_t * argv[])
{
	memset(&wifi_ssid, 0, SSID_LENGTH+1);
	EEPROM_Read(SSID_ADDRESS, wifi_ssid, SSID_LENGTH);
	uart_add_to_buffer((char *)wifi_ssid);
	send_data();
}

//obs³uga komendy wf-ssid - bez argumentu odczyt nazwy z EEPROM, z argumentem zapis nowej nazwy do EEPROM
void wifi_ssid_command(uint8_t argc, uint8_t * argv[])
{
	if (argc == 2)																//z argumentem - zapis nowego SSID
	{
		EEPROM_Write(SSID_ADDRESS, (uint8_t *)argv[1], SSID_LENGTH);
		uart_buffer_clear();
		uart_add_to_buffer("SSID set: ");
		wifi_ssid_read(argc, argv);
	}
	else if (argc == 1)															//bez argumentu - odczyt aktualnego SSID
	{
		uart_buffer_clear();
		uart_add_to_buffer("Current SSID: ");
		wifi_ssid_read(argc, argv);
	}
	else																		//wiêcej argumentów - wyœwietlenie pomocy
	{
		uart_buffer_clear();
		uart_add_to_buffer("wf-ssid name[string]");
		send_data();
	}
}

//odczyt has³a z eeprom
void wifi_pass_read(uint8_t argc, uint8_t * argv[])
{
	memset(&wifi_pass, 0, PASSWORD_LENGTH+1);
	EEPROM_Read(PASSWORD_ADDRESS, wifi_pass, PASSWORD_LENGTH);
	uart_add_to_buffer((char *)wifi_pass);
	send_data();
}

//obs³uga komendy wf-pass - bez argumentu odczyt has³a z EEPROM, z argumentem zapis nowego has³a do EEPROM
void wifi_pass_command(uint8_t argc, uint8_t * argv[])
{
	if (argc == 2)																//z argumentem - zapis nowego has³a
	{
		EEPROM_Write(PASSWORD_ADDRESS, (uint8_t *)argv[1], PASSWORD_LENGTH);
		uart_buffer_clear();
		uart_add_to_buffer("Password set: ");
		wifi_pass_read(argc, argv);
	}
	else if (argc == 1)															//bez argumentu - odczyt aktualnego has³a
	{
		uart_buffer_clear();
		uart_add_to_buffer("Current password: ");
		wifi_pass_read(argc, argv);
	}
	else																		//wiêcej argumentów - wyœwietlenie pomocy
	{
		uart_buffer_clear();
		uart_add_to_buffer("wf-ssid name[string]");
		send_data();
	}
}

//polecenie konfiguracji wifi przez WPSa
void wifi_wps_command(uint8_t argc, uint8_t * argv[])
{
	if (check_wifi_flags() == NO_FLAGS)					//sprawdzenie flag (czy nie jest wykonywane inne polecenie wymagaj¹ce wi-fi)
	{
		//ustawienie flagi i wykonanie funkcji
		WPS_flag = true;												//sprawdziæ czy flaga mo¿e byæ w tym miejscu (callbacki)
		uart_buffer_clear();
		uart_add_to_buffer("OK");
		send_data();
		wps_connect();
	}
}

//polecenie konfiguracji urz¹dzenia przez stronê w przegl¹darce
void wifi_config_command(uint8_t argc, uint8_t * argv[])
{
	if (check_wifi_flags() == NO_FLAGS)
	{
		http_config_flag = true;
		uart_add_to_buffer("OK");
		send_data();
		wifi_config_http();
	}
}

//przerwanie konfiguracji poprzez stronê
void wifi_cancel_config_command(uint8_t argc, uint8_t * argv[])
{
	uart_buffer_clear();
	if (http_config_flag)									//jeœli strona by³a uruchomiona - przerwij
	{
		uart_add_to_buffer("OK");
		wifi_cancel_config();
	}
	else													//jeœli nie by³a uruchomiona - komunikat
	{
		uart_add_to_buffer("Config page not started");
	}
	send_data();
}

//wykonanie polecenia wyszukiwania sieci (w celu okreœlenia mocy sygna³u sieci zapisanej w pamiêci)
void wifi_scan_command(uint8_t argc, uint8_t * argv[])
{
	if (check_wifi_flags() == NO_FLAGS)			//sprawdzenie flag wifi
	{
		uart_add_to_buffer("OK");
		send_data();
		scanning_flag = true;
		wifi_scan();
	}
}


//inicjalizacja i2c
void i2c_init(uint8_t argc, uint8_t * argv[])
{
	uart_buffer_clear();
	
	if (i2c_flag == false)								//zabezpieczenie przed podwójnym uruchomieniem
	{
		i2c_config();									//konfiguracja parametrów komunikacji
		i2c_config_callback();							//konfiguracja callbacków
		i2c_flag = true;								//zmiana wartoœci flagi
		uart_add_to_buffer("OK");
		if (command_pointer != i2c_read_temp_command && command_pointer != i2c_scan_command) send_data();
	}
	else												//jeœli by³o uruchomione wczeœniej - komunikat
	{
		uart_add_to_buffer("I2C initialized\r\n");
		send_data();
	}
}

//wy³¹czenie i2c
void i2c_deinit(uint8_t argc, uint8_t * argv[])
{
	uart_buffer_clear();
	
	if (i2c_flag)									//zabezpieczenie przed podwójnym uruchomieniem
	{
		i2c_disable();											//wy³¹czenie komunikacji
		i2c_flag = false;										//zmiana wartoœci flagi
		uart_add_to_buffer("OK");
	}
	else uart_add_to_buffer("I2C closed");			//jeœli i2c nie by³o uruchomione
	send_data();
}

//obs³uga komendy i2c-temp - odczyt temperatury z termometru pod³¹czonego po i2c
void i2c_read_temp_command(uint8_t argc, uint8_t * argv[])
{
	if (i2c_flag == false) i2c_init(argc, argv);
	if (i2c_flag)
	{
		i2c_read_temp();									//odczyt temperatury z adresu termometru AVR-IoT
	}
}

//obs³uga komendy skanowania wszystkich urz¹dzeñ po³¹czonych po I2C
void i2c_scan_command(uint8_t argc, uint8_t * argv[])
{
	if (i2c_flag == false) i2c_init(argc, argv);
	if (i2c_flag)
	{
		i2c_scan();						//sprawdzenie czy I2C jest uruchomione i rozpoczêcie skanowania
		i2c_deinit(argc, argv);
	}
}

//rozpoczêcie po³¹czenia przez SSL
void ssl_connect_command(uint8_t argc, uint8_t * argv[])
{
	if (check_wifi_flags() == NO_FLAGS)							//sprawdzenie flag (czy atwinc nie jest ju¿ zajêty)
	{
		ssl_flag = true;										//sprawdziæ czy flaga mo¿e byæ w tym miejscu (callbacki)
		ssl_connect(argc, argv);
	}
}

//obs³uga polecenia pobrania czasu z serwera NTP
void NTP_command(uint8_t argc, uint8_t * argv[])
{
	if (check_wifi_flags() == NO_FLAGS)							//sprawdzenie flag (czy atwinc nie jest zajêty)
	{
		NTP_sync_flag = true;
		
		uart_add_to_buffer("OK");
		send_data();
		ssl_connect(argc, argv);
	}
}


//polecenie rozpoczêcia budowy logu wysy³anego na serwer, polecenie rozpoznawane w callbacku ssl_socket_cb
void log_trigger_command(uint8_t argc, uint8_t * argv[])
{
	uart_buffer_clear();
	uart_add_to_buffer("OK");
	send_data();
	ssl_connect_command(argc, argv);										//po³¹czenie z AWS (nazwa hosta i port wczeœniej zapisane w przyk³adowym programie)
}

//polecenie dodania wpisu w logu
void log_item_command(uint8_t argc, uint8_t * argv[])
{
	if (argc == 2)
	{
		uint8_t i = 0;
		while (*argv[1] != 0)
		{
			log_uart_buf[i++] = *argv[1]++;
		}
		uart_buffer_clear();
		uart_add_to_buffer("OK");
		send_data();
	}
}

//polecenie zakoñczenia budowy logu, nastêpnie wys³anie na serwer, polecenie rozpoznane w ssl_socket_cb?
void log_end_command(uint8_t argc, uint8_t * argv[])
{
	uart_buffer_clear();
	send_data();
}

//polecenie po³¹czenia przez MQTT
void mqtt_connect_command(uint8_t argc, uint8_t * argv[])
{
	if (check_wifi_flags() == NO_FLAGS)			//sprawdzenie flag (czy atwinc nie jest zajêty)
	{
		mqtt_flag = true;
		aws_mqtt_init();
	}
}

//wykonanie polecenia zakoñczenia po³¹czenia przez MQTT
void mqtt_close_command(uint8_t argc, uint8_t * argv[])
{
	uart_buffer_clear();
 	if (mqtt_flag)									//sprawdzenie czy MQTT zosta³o uruchomione
 	{
 		mqtt_close_flag = true;						//wystawienie flagi zamkniêcia komunikacji po MQTT, samo zamkniêcie dokonane w main
 	}
 	else
	{
		uart_add_to_buffer("MQTT not connected");
		send_data();
	}
}

//wykonanie polecenia echo - odes³anie argumentu przez UART
void echo_command(uint8_t argc, uint8_t * argv[])
{
	char string[256];
	sprintf(string, "%s", (const char *)argv[1]);
	uart_buffer_clear();
	uart_add_to_buffer(string);
	send_data();
}

//polecenie konfiguracji sposobu pobierania listy kart (1 dla MQTT, 0 dla SSL, domyœlnie 0 - SSL)
void list_mqtt_command(uint8_t argc, uint8_t * argv[])
{
	if (argc == 2)						//z argumentem - konfiguracja
	{
		if (*argv[1] == '0')			//0 - SSL
		{
			uart_buffer_clear();
			list_mqtt_flag = false;
			uart_add_to_buffer("Using SSL");
		}
		else if (*argv[1] == '1')		//1 - MQTT
		{
			uart_buffer_clear();
			list_mqtt_flag = true;
			uart_add_to_buffer("Using MQTT");
		}
		else							//inny argument - b³¹d
		{
			uart_buffer_clear();
			uart_add_to_buffer("Bad argument");
		}
	}
	else if (argc == 1)					//jeœli bez argumentu - wyœwietl aktualn¹ konfiguracjê
	{
		uart_buffer_clear();
		if (list_mqtt_flag == false) uart_add_to_buffer("Using SSL");
		else uart_add_to_buffer("Using MQTT");
	}
	else								//inna liczba argumentów - wyœwietl pomoc
	{
		uart_buffer_clear();
		uart_add_to_buffer("list-mqtt [ 0 / 1 ]");
	}
	send_data();
}

//wykonanie polecenia echo-loop (uruchomienie lub wy³¹czenie pêtli wysy³aj¹cej komendê echo)
void echo_loop_command(uint8_t argc, uint8_t * argv[])
{
	echo_loop_flag = !echo_loop_flag;
	uart_buffer_clear();
	uart_add_to_buffer("OK");
	send_data();
}

//obliczenie sumy kontrolnej CRC
void CRC_command(uint8_t argc, uint8_t * argv[])
{
	uint8_t length = strlen((const char *)argv[1]);
	printf("L:%u_", length);

	uint16_t CRC = crc_calculate(argv[1], length);

	printf("CRC:%04X", CRC);
}

//funkcja sprawdzaj¹ca flagi wifi (czy atwinc jest zajêty)
wifi_flags check_wifi_flags(void)
{
	if (NTP_sync_flag)
	{
		uart_buffer_clear();
		uart_add_to_buffer("Time sync in progress");
		send_data();
		return FLAG_SET;
	}
	else if (mqtt_flag)
	{
		uart_buffer_clear();
		uart_add_to_buffer("MQTT connected");
		send_data();
		return FLAG_SET;
	}
	else if (ssl_flag)
	{
		uart_buffer_clear();
		uart_add_to_buffer("SSL connected");
		send_data();
		return FLAG_SET;
	}
	else if (WPS_flag)
	{
		uart_buffer_clear();
		uart_add_to_buffer("WPS connecting");
		send_data();
		return FLAG_SET;
	}
	else if (http_config_flag)
	{
		uart_buffer_clear();
		uart_add_to_buffer("Config page started");
		send_data();
		return FLAG_SET;
	}
	else if (scanning_flag)
	{
		uart_buffer_clear();
		uart_add_to_buffer("Scanning started");
		send_data();
		return FLAG_SET;
	}
	else
	{
		return NO_FLAGS;
	}
}

//polecenie odczytu root CA i certyfikatu TLS z pamiêci atwinca
void cert_read_command(uint8_t argc, uint8_t * argv[])
{
	cert_read();
//	spi_flash_read(buffer_usart.buffer, M2M_TLS_ROOTCER_FLASH_OFFSET, FLASH_SECTOR_SZ * 3);
	//odczytaj dane z konkretnego sektora atwina, opis w spi_flash_map
}

//funkcja zapisu certyfikatu TLS przez UART
void cert_write_command(uint8_t argc, uint8_t * argv[])
{
	cert_write_flag = true;
}

//polecenie odczytania obszaru pamiêci atwinca ze stron¹ konfiguracyjn¹
void provision_page_read_command(uint8_t argc, uint8_t * argv[])
{
	provision_page_read();
}

//polecenie przywrócenia ustawieñ fabrycznych (SSID i has³o)
void factory_reset_command(uint8_t argc, uint8_t * argv[])
{
	factory_reset();
}

//polecenie zapisania root CA przez UART
void root_cert_write_command(uint8_t argc, uint8_t * argv[])
{
	root_cert_default();
}

//polecenie generowania CSR - testowe dla ATECC608
void generate_csr_command(uint8_t argc, uint8_t * argv[])
{
//	generate_csr();
}

void mqtt_subscribe_command(uint8_t argc, uint8_t * argv[])
{
	if (!mqtt_flag)
	{
		mqtt_connect_command(argc, argv);
		//konfiguracja timera
	
		//timer uruchomiony po otrzymaniu i przerobieniu odpowiedzi od AWSa, 
	
	
	
		//uruchomienie MQTT
		//subskrybowanie
		//wystawienie flagi
		//uruchomienie timera który co np. 10 sekund przepytuje AWSa
	}
}

void SSL_subscribe_command(uint8_t argc, uint8_t * argv[])
{
	//po³¹czenie przez SSL
	//wystawienie flagi
	//wysy³anie requesta w tym samym timerze co np. 10 sekund
}


#if C_FLASH
void spi_samd_flash_clear_command(uint8_t argc, uint8_t * argv[])
{
	spi_samd_flash_clear();
}

void spi_samd_flash_write_command(uint8_t argc, uint8_t * argv[])
{
	if(argc == 3)
	{
		//dodaæ sprawdzanie czy argumenty podane s¹ w HEX
		uint32_t address = 0;
		
		if(strlen((const char *)argv[1]) == 6) parse_HEX24(argv[1], &address);
		else
		{
			uart_send_buffer((uint8_t *)"ADDRESS HEX24");
			return;
		}
		
		//zapobiegaæ przekroczeniu granicy stron (jedna strona - 256 adresów)
		//maksymalnie 128 bajtów w jednym procesie zapisu
		
		uint8_t length = strlen((const char *)argv[2]) / 2;
		
		for (uint8_t i = 0; i < length; i++)
		{
			if (i < length) parse_HEX8((argv[2]+(2*i)), (argv[2]+i));
			else *(argv[2]+i) = 0;
		}
		spi_samd_flash_write(address, argv[2], length);
// 		uint8_t byte_read = 0;
// 		spi_samd_flash_read(&address, &byte_read);
	}
	else uart_send_buffer((uint8_t *)"flash-write address[HEX24] data[HEX]");
}

void spi_samd_flash_read_command(uint8_t argc, uint8_t * argv[])
{
	if(argc == 2)
	{
		uint32_t address = 0;
		if(strlen((const char *)argv[1]) == 6) parse_HEX24(argv[1], &address);
		else
		{
			uart_send_buffer((uint8_t *)"ADDRESS HEX24");
			return;
		}
		uint8_t byte_read;
		spi_samd_flash_read(address, &byte_read);
		uart_send_hex(byte_read, 0);
	}
	else uart_send_buffer((uint8_t *)"flash-read address[HEX24]");
}

void spi_samd_flash_dump_command(uint8_t argc, uint8_t * argv[])
{
	if (argc == 3)
	{
		uint32_t address = 0;
		if(strlen((const char *)argv[1]) == 6) parse_HEX24(argv[1], &address);
		else
		{
			uart_send_buffer((uint8_t *)"ADDRESS HEX24");
			return;
		}
		
		//argv[2] - liczba danych do odczytu - w hex
		
		uint32_t length = 0;
		if(strlen((const char *)argv[2]) == 6) parse_HEX24(argv[2], &length);
		else
		{
			uart_send_buffer((uint8_t *)"LENGTH HEX24");
			return;
		}
		
		
		/*
		dla zwyk³ych znaków: zale¿nie od liczby znaków w argumencie argv[2] ró¿ne mno¿niki
		
		
		
		*/
		
		uint8_t read_buf[16];
		memset(&read_buf, 0, 16);
		spi_samd_flash_dump(address, length, read_buf);

//		uart_send_char((char)byte_read);
	}
	else uart_send_buffer((uint8_t *)"f-dump address[HEX24] length[HEX24]");
}

void spi_samd_flash_sector_command(uint8_t argc, uint8_t * argv[])
{
	if (argc == 2)
	{
		uint32_t address = 0;
		if(strlen((const char *)argv[1]) == 6) parse_HEX24(argv[1], &address);
		else
		{
			uart_send_buffer((uint8_t *)"ADDRESS HEX24");
			return;
		}
		spi_samd_flash_erase(address, SPIMEM_ERASE_SECTOR);
		uart_send_buffer((uint8_t *)"OK");
	}
	else uart_send_buffer((uint8_t *)"f-sector address[HEX24]");
}

#endif

#if C_USB
void usb_start_command(uint8_t argc, uint8_t * argv[])
{
	udc_start();
	uart_send_buffer((uint8_t *)"OK");
}

void usb_stop_command(uint8_t argc, uint8_t * argv[])
{
	udc_stop();
	uart_send_buffer((uint8_t *)"OK");
}
#endif

#if C_ATECC608
void atecc608_wake_command(uint8_t argc, uint8_t * argv[])
{
	if (i2c_flag)
	{
		I2C_Start(0x00);
	}
}

void atecc608_stop_command(uint8_t argc, uint8_t * argv[])
{
	I2C_Stop();
}

void atecc608_random_command(uint8_t argc, uint8_t * argv[])
{
	uint8_t buffer[32];
	memset(&buffer, 0, sizeof(buffer));
	
	enum ATECC608_Res Result = ATECC608_Random(buffer);
	if (Result == ATECC608_OK) uart_send_hex_string(buffer, sizeof(buffer), ' ', 16);
	else ATECC608_Debug(Result, buffer[0]);

}

void atecc608_sha256_command(uint8_t argc, uint8_t * argv[])
{
	uint8_t Length;
	uint8_t Output[32];

	if(argc == 1) uart_send_buffer((uint8_t *)"ecc-sha string[ASCII] _dest[t/m/o]\r\n");

	// Argument 2 jest opcjonalny
	uint8_t Destination;
	if(argc == 3) {
		switch(*argv[2]) {
			case 't':		Destination = ATECC608_SHA256_DEST_OUT_TEMPKEY;			break;
			case 'm':		Destination = ATECC608_SHA256_DEST_OUT_MESDIGBUF;		break;
			default:		Destination = ATECC608_SHA256_DEST_OUT_ONLY;			break;
		}
	}
	else {
		Destination = ATECC608_SHA256_DEST_OUT_ONLY;
	}

	uart_send_buffer((uint8_t *)"Length=");
	
	if(argc == 1) Length = 0;
	else Length = strlen((const char *)argv[1]);
	
	uart_send_dec(Length);
	uart_send_char(CR);
	
	enum ATECC608_Res Result = ATECC608_SHA256(Output, Destination, argv[1], Length);
	if(Result == ATECC608_OK) uart_send_hex_string(Output, sizeof(Output), ' ', 16);
	else ATECC608_Debug(Result, Output[0]);
}


// Generowanie klucza publicznego
void atecc608_GenKeyPublic_command(uint8_t argc, uint8_t * argv[])
{
	enum CmdRes_t CommandResult;
	
	if(argc == 1)
	{
		uart_send_buffer((uint8_t *)"ecc-genkeypub slot[DEC8]");
		return;
	}

	// Argument 1 - slot
	uint16_t Slot;
	CommandResult = parse_dec16(argv[1], &Slot, 65535);
	if(CommandResult) {
		//////Command_Debug(CommandResult, argv[1]);
		return;
	}
	if(Slot > 15) {				// TempKey
		Slot = ATECC608_TEMPKEY;
	}
	
	// Wykonanie polecenia
	uint8_t Key[64];
	enum ATECC608_Res Result = ATECC608_GenKeyPublic(Key, Slot);
	if(Result) {
		ATECC608_Debug(Result, Key[0]);
		return;
	}

	// Wyœwietlenie wyniku
	uart_send_hex_string(Key, sizeof(Key), ' ', 16);
}


// Generowanie klucza prywatnego
void atecc608_GenKeyPrivate_command(uint8_t argc, uint8_t * argv[])
{
	enum CmdRes_t CommandResult;
	
	if(argc == 1)
	{
		uart_send_buffer((uint8_t *)"ecc-genkeypriv slot[DEC8]");
		return;
	}

	// Argument 1 - slot
	uint16_t Slot;
	CommandResult = parse_dec16(argv[1], &Slot, 65535);
	if(CommandResult) {
		//////Command_Debug(CommandResult, argv[1]);
		return;
	}
	if(Slot > 15) {				// TempKey
		Slot = ATECC608_TEMPKEY;
	}
	
	// Wykonanie polecenia
	uint8_t Key[64];
	enum ATECC608_Res Result = ATECC608_GenKeyPrivate(Key, Slot);
	if(Result) {
		ATECC608_Debug(Result, Key[0]);
		return;
	}

	// Wyœwietlenie wyniku
	uart_send_hex_string(Key, sizeof(Key), ' ', 16);
}


// Generowanie klucza wspólnego metod¹ ECDH (Diffie-Hellman)
void atecc608_ECDH_command(uint8_t argc, uint8_t * argv[])
{
	enum CmdRes_t CommandResult;
	
	if(argc != 5)
	{
		uart_send_buffer((uint8_t *)"ecc-ecdh source[s/t] dest[s/t/o] slot[DEC8] publickey[HEXSTR64]");
		return;
	}

	// Argument 1 - klucz prywatny
	uint8_t Source;
	switch(*argv[1]) {
		case 's':		Source = ATECC608_ECDH_SOURCE_SLOT;			break;
		case 't':		Source = ATECC608_ECDH_SOURCE_TEMPKEY;		break;
		default:		uart_send_buffer((uint8_t *)"Wrong source");					return;
	}

	// Argument 2 - cel
	uint8_t Dest;
	switch(*argv[2]) {
		case 's':		Dest = ATECC608_ECDH_DEST_SLOT;				break;
		case 't':		Dest = ATECC608_ECDH_DEST_TEMPKEY;			break;
		case 'o':		Dest = ATECC608_ECDH_DEST_OUTPUT;			break;
		default:		uart_send_buffer((uint8_t *)"Wrong destination");			return;
	}

	// Argument 3 - slot
	uint16_t Slot;
	CommandResult = parse_dec16(argv[3], &Slot, 65535);
	if(CommandResult) {
		//////Command_Debug(CommandResult, argv[3]);
		return;
	}
	
	// Argument 4 - klucz publiczny
	uint8_t PublicKey[64];
	uint8_t PublicKeyLength;
	CommandResult = Parse_HexString(argv[4], PublicKey, &PublicKeyLength, 64, 64);
	if(CommandResult) {
		//////Command_Debug(CommandResult, argv[4]);
		return;
	}
	
	// Wykonanie polecenia
	uint8_t Mode = Source | Dest | ATECC608_ECDH_ENCODING_OFF;
	uint8_t SharedSecret[32];
	memset(SharedSecret, 0, sizeof(SharedSecret));
	enum ATECC608_Res Result = ATECC608_ECDH(SharedSecret, Mode, Slot, PublicKey);
	if(Result) {
		ATECC608_Debug(Result, SharedSecret[0]);
		return;
	}

	// Wyœwietlenie wyniku
	if(Dest == ATECC608_ECDH_DEST_OUTPUT) {
		uart_send_hex_string(SharedSecret, sizeof(SharedSecret), ' ', 16);
	}
	else {
		uart_send_buffer((uint8_t *)"Ok");
	}
}

// Generowanie podpisu cyfrowego ECDSA
void atecc608_Sign_command(uint8_t argc, uint8_t * argv[])
{
	uint8_t Output[64];
	uint8_t Mode;
	uint16_t Key;
	enum CmdRes_t ResultCommand;

	if(argc < 3)
	{
		uart_send_buffer((uint8_t *)"ecc-sign key[DEC8] source[1/2/t/m] _validate[1/0]");
		return;
	}

	// Argument 1 - wybór klucza
	ResultCommand = parse_dec16(argv[1], &Key, 15);
	if(ResultCommand) {
		//////Command_Debug(ResultCommand, argv[1]);
		return;
	}

	// Argument 2 - wybór Ÿród³a (czytaj tabela 11-50)
	switch(*argv[2]) {
		case '1':	Mode = ATECC608_SIGN_MESSAGE_INT_1;				break;
		case '2':	Mode = ATECC608_SIGN_MESSAGE_INT_2;				break;
		case 't':	Mode = ATECC608_SIGN_MESSAGE_EXT_TEMPKEY;		break;
		case 'm':	Mode = ATECC608_SIGN_MESSAGE_EXT_MESDIGBUF;		break;
		default:	uart_send_buffer((uint8_t *)"Wrong source");	return;
	}

	// Argument 3 opcjonalny - tryub walidacji
	if(argc == 4 && *argv[3] == '0') {
		Mode = Mode | ATECC608_SIGN_VERIFF_INVALIDATE;
	}

	// Wykonanie polecenia
	enum ATECC608_Res Result = ATECC608_Sign(Output, Mode, Key);

	// Wyœwietlanie wytniku
	if(Result == ATECC608_OK) {
		uart_send_hex_string(Output, sizeof(Output), ' ', 16);
	}
	else {
		ATECC608_Debug(Result, Output[0]);
	}
}


// Weryfikacja podpisu cyfrowego ECDSA
void atecc608_Verify_command(uint8_t argc, uint8_t * argv[])
{
	uint8_t Output;
	uint8_t SignatureRS[64];
	uint8_t SignatureLength;
	uint8_t PubKeyXY[64];
	uint8_t PubKeyXYLength;
	uint8_t Mode;
	uint16_t Key;
	enum CmdRes_t ResultCommand;

	if(argc == 1)
	{
		uart_send_buffer((uint8_t *)"ecc-verify mode[e/s/v/i/5] signature[HEXSTR64] pubkeyslot[DEC8]/pubkeyxy[HEXSTR64] digestsource[t/m]");
		return;
	}

	// Argument 1 - tryb pracy
	switch(*argv[1]) {
		case 'e':	Mode = ATECC608_VERIFY_MODE_EXTERNAL;	break;
		case 's':	Mode = ATECC608_VERIFY_MODE_STORED;		break;
		default:	uart_send_buffer((uint8_t *)"Mode not supported");		return;
	}

	// Argument 2 - sygnatura
	ResultCommand = Parse_HexString(argv[2], SignatureRS, &SignatureLength, 64, 64);
	if(ResultCommand) {
		//////Command_Debug(ResultCommand, argv[2]);
		return;
	}

	// Argument 3 w zale¿noœci od trybu
	if(Mode == ATECC608_VERIFY_MODE_STORED) {
		// Je¿eli stored, to klucz jest zapisany w EEPROM, pobieramy numer slotu
		ResultCommand = parse_dec16(argv[3], &Key, 15);
		if(ResultCommand) {
			//////Command_Debug(ResultCommand, argv[3]);
			return;
		}
	}
	else {
		// Je¿eli external, to klucz jest danych przez argument
		ResultCommand = Parse_HexString(argv[3], PubKeyXY, &PubKeyXYLength, 64, 64);
		if(ResultCommand) {
			//////Command_Debug(ResultCommand, argv[3]);
			return;
		}

		// W tym przypadku Key to definicja krzywej eliptycznej u¿ytej do weryfikacji
		Key = ATECC608_KEYTYPE_P256_NIST_ECC;
	}

	// Argument 4 - Ÿród³o digestu wiadomoœci dla trybu pracy stored i external
	if(argc == 5) {
		switch(*argv[4]) {
			case 't':	Mode |= ATECC608_VERIFY_SOURCE_TEMPKEY;		break;
			case 'm':	Mode |= ATECC608_VERIFY_SOURCE_MESDIGBUF;	break;
			default:	uart_send_buffer((uint8_t *)"Wrong digest source");			break;
		}
	}

	// P³ytka AVR-IoT ma wy³¹czony IO Protection
	//Mode |= ATECC608_VERIFY_MAC_ADD;

	// Wykonanie polecenia
	enum ATECC608_Res Result = ATECC608_Verify(&Output, Mode, Key, SignatureRS, PubKeyXY);

	// Wyœwietlanie wytniku
	ATECC608_Debug(Result, Output);
}

void atecc608_CSR_command(uint8_t argc, uint8_t * argv[])
{
	if (argc == 1) ATECC608_CSR();
}

#endif