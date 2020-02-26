#ifndef _COMMANDS_LIST_H_
#define _COMMANDS_LIST_H_

#include "commands.h"
#include "connect_wifi.h"

const struct commands_struct commands_list[] = {
// 	{"mqtt-connect",	mqtt_sub_command},
// 	{"mqtt-close",		mqtt_close_command},
//	{"mqtt-sub",		mqtt_subscribe_command},
//	{"ssl-sub",			SSL_subscribe_command},
	{"serial",			serial_command},
	{"w-led",			led_switch},
	{"list-download",	list_download_command},
	{"list-get",		list_get_command},
	{"ssl-connect",		ssl_connect_command},
	{"ssl-close",		ssl_close_command},
	{"w-ssid",			wifi_ssid_command},
	{"w-pass",			wifi_pass_command},
	{"wps-init",		wifi_wps_command},
	{"w-config",		wifi_config_command},
	{"w-cancel",		wifi_cancel_config_command},
	{"get-rssi",		wifi_scan_command},
	{"csr-get",			generate_csr_command},
	{"i2c-init",		i2c_init},
	{"i2c-close",		i2c_deinit},
	{"i2c-temp",		i2c_read_temp_command},
	{"i2c-scan",		i2c_scan_command},
	{"time-get",		NTP_command},
	{"log-trg",			log_trigger_command},
	{"log-item",		log_item_command},
	{"log-end",			log_end_command},
	{"mqtt-connect",	mqtt_connect_command}, 
	{"mqtt-close",		mqtt_close_command},
	{"echo",			echo_command},
	{"list-mqtt",		list_mqtt_command},
	{"echo-loop",		echo_loop_command},
	{"CRC",				CRC_command},
	{"cert-read",		cert_read_command},
	{"cert-write",		cert_write_command},
	{"prov-read",		provision_page_read_command},
	{"root-write",		root_cert_write_command},
	{"iks-de",			factory_reset_command},
	
	#if C_FLASH
	{"f-write",			spi_samd_flash_write_command},
	{"f-read",			spi_samd_flash_read_command},
	{"f-dump",			spi_samd_flash_dump_command},
	{"f-clear",			spi_samd_flash_clear_command},
	{"f-sector",		spi_samd_flash_sector_command},
	#endif
	
	#if C_USB
	{"usb-start",		usb_start_command},
	{"usb-stop",		usb_stop_command},
	#endif
	
	#if C_ATECC608
//	{"ecc-wake",		atecc608_wake_command},
//	{"ecc-stop",		atecc608_stop_command},
	{"ecc-rand",		atecc608_random_command},
	{"ecc-sha",			atecc608_sha256_command},
	{"ecc-genkeypriv",	atecc608_GenKeyPrivate_command},
	{"ecc-genkeypub",	atecc608_GenKeyPublic_command},
	{"ecc-ecdh",		atecc608_ECDH_command},
	{"ecc-sign",		atecc608_Sign_command},
	{"ecc-verify",		atecc608_Verify_command},
	{"ecc-csr",			atecc608_CSR_command},
	#endif
};

#endif // _COMMANDS_LIST_H_