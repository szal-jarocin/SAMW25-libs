#include <bod.h>
#include <eeprom.h>
#include <interrupt.h>
#include <system_interrupt.h>
#include <string.h>
#include "eeprom_emul.h"
#include "connect_wifi.h"
#include "commands.h"
#include "atwinc_flash.h"

extern uint8_t serial_num[SERIAL_LENGTH+1];
extern uint8_t wifi_ssid[SSID_LENGTH+1];
extern uint8_t wifi_pass[PASSWORD_LENGTH+1];

//konfiguracja EEPROM - z biblioteki z ASF
void configure_eeprom(void)
{
	/* Setup EEPROM emulator service */
	enum status_code error_code = eeprom_emulator_init();

	if (error_code == STATUS_ERR_NO_MEMORY) {
		while (true) {
			/* No EEPROM section has been set in the device's fuses */
		}
	}
	else if (error_code != STATUS_OK) {
		/* Erase the emulated EEPROM memory (assume it is unformatted or
		 * irrecoverably corrupt) */
		eeprom_emulator_erase_memory();
		eeprom_emulator_init();
	}
}

#if (SAMD || SAMR21)
void SYSCTRL_Handler(void)
{
	if (SYSCTRL->INTFLAG.reg & SYSCTRL_INTFLAG_BOD33DET) {
		SYSCTRL->INTFLAG.reg = SYSCTRL_INTFLAG_BOD33DET;
		eeprom_emulator_commit_page_buffer();
	}
}
#endif

static void configure_bod(void)
{
	#if (SAMD || SAMR21)
	struct bod_config config_bod33;
	bod_get_config_defaults(&config_bod33);
	config_bod33.action = BOD_ACTION_INTERRUPT;
	/* BOD33 threshold level is about 3.2V */
	config_bod33.level = 48;
	bod_set_config(BOD_BOD33, &config_bod33);
	bod_enable(BOD_BOD33);

	SYSCTRL->INTENSET.reg = SYSCTRL_INTENCLR_BOD33DET;
	system_interrupt_enable(SYSTEM_INTERRUPT_MODULE_SYSCTRL);
	#endif

}

//konfiguracja EEPROM - wywo�anie w mainie, dalej odniesienie do funkcji z eeprom_emul.c
void EEPROM_emul_config(void)
{
	configure_eeprom();
	configure_bod();
}

//funkcja odczytania danych z EEPROM z podanego adresu - odczytuje 30 bajt�w (do modyfikacji zale�nie od d�ugo�ci nazw i hase�)
void EEPROM_Read(uint8_t Address, uint8_t Data[], uint8_t length)
{
	//pobranie adresu i na tej podstawie ustalenie offsetu wewn�trz funkcji
	eeprom_emulator_read_buffer(Address, Data, length);								//sprawdzi� rozmiar do odczytu has�a i nazwy w jednym warunku? Rozmiar maksymalny i sprawdzanie do nulla?
}

//zapisanie danych do EEPROM - j.w.
void EEPROM_Write(uint8_t Address, uint8_t *Data, uint8_t length)
{
	eeprom_emulator_write_buffer(Address, Data, length);				//zapisanie nowego stanu do eeprom
	eeprom_emulator_commit_page_buffer();
}

//zapisanie domy�lnych warto�ci do pami�ci EEPROM - aktualnie zakomentowane
void EEPROM_save_defaults(void)
{
//	EEPROM_Write(SSID_ADDRESS, (uint8_t *)"", SSID_LENGTH);
//	EEPROM_Write(PASSWORD_ADDRESS, (uint8_t *)"", PASSWORD_LENGTH);
	
	memset(serial_num, 0, SERIAL_LENGTH+1);
	EEPROM_Read(SERIAL_ADDRESS, serial_num, SERIAL_LENGTH);
	if (serial_num[0] == 255)
	{
		//ustawienie domy�lnego seriala dla nowego czytnika
		EEPROM_Write(SERIAL_ADDRESS, (uint8_t *)"DEFAULT", SERIAL_LENGTH);
		
		//zapisanie strony konfiguracyjnej z programowej tablicy do atwinca
		//provision_page_default();
		
		//zapisanie domy�lnych root cert�w z tablicy programowej do ATWINCa
		//root_cert_default();
	}
	memset(wifi_ssid, 0, SSID_LENGTH+1);
	EEPROM_Read(SSID_ADDRESS, wifi_ssid, SSID_LENGTH);
	memset(wifi_pass, 0, PASSWORD_LENGTH+1);
	EEPROM_Read(PASSWORD_ADDRESS, wifi_pass, PASSWORD_LENGTH);
}

//przywr�cenie domy�lnych ustawie� po wpisaniu polecenia resetu
void factory_reset(void)
{
	memset(wifi_ssid, 0, SSID_LENGTH+1);
	EEPROM_Write(SSID_ADDRESS, wifi_ssid, SSID_LENGTH);
	memset(wifi_pass, 0, PASSWORD_LENGTH+1);
	EEPROM_Write(PASSWORD_ADDRESS, wifi_pass, PASSWORD_LENGTH);
}