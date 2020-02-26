#ifndef _LED_H_
#define _LED_H_

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

typedef enum {
	MANAGEMENT            = 0x00,
	/*!< Wi-Fi Management frame (Probe Req/Resp, Beacon, Association Req/Resp ...etc).
	*/
	CONTROL               = 0x04,
	/*!< Wi-Fi Control frame (eg. ACK frame).
	*/
	DATA_BASICTYPE        = 0x08,
	/*!< Wi-Fi Data frame.
	*/
	RESERVED              = 0x0C,

	M2M_WIFI_FRAME_TYPE_ANY	= 0xFF
/*!< Set monitor mode to receive any of the frames types
*/
}tenuWifiFrameType;


//typ wykorzystywany przy wyszukiwaniu flag wifi
typedef enum {
	NO_FLAGS = 0,
	FLAG_SET = 1
}wifi_flags;

// Typ wynikowy dla funkcji interpretuj¹cych znaki
enum CmdRes_t {
	Cmd_OK = 0,									// Zwracane przez wszystkie funkcje, je¿eli zakoñczy³y siê prawid³owo
	Cmd_NotReady,								// Zwracane przez Command_LineInput(), kiedy nie podano znaku ENTER
	Cmd_UnknownCommand,							// Zwracane przez Command_Interpreter(), kiedy nie rozpozna polecenia
	Cmd_NoInput,
	Cmd_MissingArgument,
	Cmd_Overflow,
	Cmd_Underflow,
	Cmd_ParseError,
	Cmd_ExpectedHex,
	Cmd_ExpectedDec,
	Cmd_ReceivedACK,							// Zwracane przez Command_Interpreter(), kiedy zostanie otrzymane potwierdzenie odebrania polecenia z po³¹czonego uk³adu
	Cmd_ReceivedNAK,							// Zwracane przez Command_Interpreter(), kiedy NIE zostanie otrzymane potwierdzenie odebrania polecenia z po³¹czonego uk³adu
};

//extern char topic[64];
extern char mqtt_user[64];
extern bool mqtt_close_flag;
extern bool NTP_sync_flag;

void led_switch(uint8_t argc, uint8_t * argv[]);
wifi_flags check_wifi_flags(void);

//obs³uga flasha ATWINC1500
void cert_read_command(uint8_t argc, uint8_t * argv[]);
void cert_write_command(uint8_t argc, uint8_t * argv[]);
void provision_page_read_command(uint8_t argc, uint8_t * argv[]);
void root_cert_write_command(uint8_t argc, uint8_t * argv[]);

//obs³uga listy kart
void list_download_command(uint8_t argc, uint8_t * argv[]);
void list_get_command(uint8_t argc, uint8_t * argv[]);

//obs³uga parametrów po³¹czenia wi-fi
void wifi_ssid_command(uint8_t argc, uint8_t * argv[]);
void wifi_pass_command(uint8_t argc, uint8_t * argv[]);
void wifi_wps_command(uint8_t argc, uint8_t * argv[]);
void serial_command(uint8_t argc, uint8_t * argv[]);
void wifi_config_command(uint8_t argc, uint8_t * argv[]);
void wifi_cancel_config_command(uint8_t argc, uint8_t * argv[]);
void wifi_scan_command(uint8_t argc, uint8_t * argv[]);
void factory_reset_command(uint8_t argc, uint8_t * argv[]);

//po³¹czenie przez SSL
void ssl_connect_command(uint8_t argc, uint8_t * argv[]);
void SSL_subscribe_command(uint8_t argc, uint8_t * argv[]);

//odczyt z EEPROM
void wifi_ssid_read(uint8_t argc, uint8_t * argv[]);
void wifi_pass_read(uint8_t argc, uint8_t * argv[]);
void serial_read(uint8_t argc, uint8_t * argv[]);
//void mqtt_sub_command(uint8_t argc, uint8_t * argv[]);
//void mqtt_close_command(uint8_t argc, uint8_t * argv[]);

//I2C
void i2c_init(uint8_t argc, uint8_t * argv[]);
void i2c_deinit(uint8_t argc, uint8_t * argv[]);
void i2c_read_temp_command(uint8_t argc, uint8_t * argv[]);
void i2c_scan_command(uint8_t argc, uint8_t * argv[]);

//synchronizacja czasu
void NTP_command(uint8_t argc, uint8_t * argv[]);

//obs³uga logów
void log_trigger_command(uint8_t argc, uint8_t * argv[]);
void log_item_command(uint8_t argc, uint8_t * argv[]);
void log_end_command(uint8_t argc, uint8_t * argv[]);

//obs³uga MQTT
void mqtt_connect_command(uint8_t argc, uint8_t * argv[]);
void mqtt_close_command(uint8_t argc, uint8_t * argv[]);
void list_mqtt_command(uint8_t argc, uint8_t * argv[]);
void mqtt_subscribe_command(uint8_t argc, uint8_t * argv[]);

//funkcje ECHO
void echo_command(uint8_t argc, uint8_t * argv[]);
void echo_loop_command(uint8_t argc, uint8_t * argv[]);

//funkcje obliczaj¹ce sumy kontrolne
void CRC_command(uint8_t argc, uint8_t * argv[]);


//obs³uga SPI FLASH
#if C_FLASH
void spi_samd_flash_clear_command(uint8_t argc, uint8_t * argv[]);
void spi_samd_flash_write_command(uint8_t argc, uint8_t * argv[]);
void spi_samd_flash_read_command(uint8_t argc, uint8_t * argv[]);
void spi_samd_flash_dump_command(uint8_t argc, uint8_t * argv[]);
void spi_samd_flash_sector_command(uint8_t argc, uint8_t * argv[]);
#endif //C_FLASH


//obs³uga USB
#if C_USB
void usb_start_command(uint8_t argc, uint8_t * argv[]);
void usb_stop_command(uint8_t argc, uint8_t * argv[]);
#endif //C_USB


//obs³uga ATECC608A
#if C_ATECC608
void generate_csr_command(uint8_t argc, uint8_t * argv[]);
void atecc608_wake_command(uint8_t argc, uint8_t * argv[]);
void atecc608_stop_command(uint8_t argc, uint8_t * argv[]);
void atecc608_random_command(uint8_t argc, uint8_t * argv[]);
void atecc608_sha256_command(uint8_t argc, uint8_t * argv[]);
void atecc608_GenKeyPrivate_command(uint8_t argc, uint8_t * argv[]);
void atecc608_GenKeyPublic_command(uint8_t argc, uint8_t * argv[]);
void atecc608_ECDH_command(uint8_t argc, uint8_t * argv[]);
void atecc608_Sign_command(uint8_t argc, uint8_t * argv[]);
void atecc608_Verify_command(uint8_t argc, uint8_t * argv[]);
void atecc608_CSR_command(uint8_t argc, uint8_t * argv[]);
#endif //C_ATECC608



#endif // _LED_H_