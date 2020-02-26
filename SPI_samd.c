#include "SPI_samd.h"
#include "SPI_mem.h"
#include "spi.h"
#include "spi_interrupt.h"
#include "delay.h"
#include "interpreter.h"

//struktury
struct spi_slave_inst slave_flash;
struct spi_module spi_master_instance;

//deklaracje funkcji
void SPI_config_samd(void);
void SPI_config_callbacks(void);



//definicje funkcji
// static void SPI_write_callback(void)
// {
// 	
// }

static void SPI_read_callback(struct spi_module *const module)
{
	//wersje dla obu przypadków - flash i RC522?
	
	
}

void SPI_config_samd(void)
{
	//z przyk³adu ASF
	struct spi_config config_spi_master;
	struct spi_slave_inst_config slave_flash_config;
	
	spi_slave_inst_get_config_defaults(&slave_flash_config);
	slave_flash_config.ss_pin = CONF_MASTER_SS_PIN;
	spi_attach_slave(&slave_flash, &slave_flash_config);
	spi_get_config_defaults(&config_spi_master);
	config_spi_master.mode_specific.master.baudrate = 1000000;				//czêstotliwoœæ 10 MHz
	config_spi_master.mux_setting = CONF_MASTER_MUX_SETTING;
	config_spi_master.pinmux_pad0 = CONF_MASTER_PINMUX_PAD0;
	config_spi_master.pinmux_pad1 = CONF_MASTER_PINMUX_PAD1;
	config_spi_master.pinmux_pad2 = CONF_MASTER_PINMUX_PAD2;
	config_spi_master.pinmux_pad3 = CONF_MASTER_PINMUX_PAD3;
	
	struct system_pinmux_config spi_pinmux;
	system_pinmux_get_config_defaults(&spi_pinmux);
	spi_pinmux.mux_position = 2;						//pinmux konfiguracja C - SERCOM						
	spi_pinmux.direction = SYSTEM_PINMUX_PIN_DIR_OUTPUT;
	system_pinmux_pin_set_config(CONF_MASTER_PINMUX_PAD0, &spi_pinmux);
	port_pin_set_output_level(CONF_MASTER_PINMUX_PAD0, true);
	system_pinmux_pin_set_config(CONF_MASTER_PINMUX_PAD3, &spi_pinmux);
	port_pin_set_output_level(CONF_MASTER_PINMUX_PAD3, true);
	spi_pinmux.input_pull = SYSTEM_PINMUX_PIN_PULL_NONE;
	spi_pinmux.direction = SYSTEM_PINMUX_PIN_DIR_INPUT;
	system_pinmux_pin_set_config(CONF_MASTER_PINMUX_PAD2, &spi_pinmux);
	
	spi_init(&spi_master_instance, CONF_MASTER_SPI_MODULE, &config_spi_master);
	
	spi_enable(&spi_master_instance);
}

void SPI_config_callbacks(void)
{
//	spi_register_callback(&spi_master_instance, SPI_write_callback, SPI_CALLBACK_BUFFER_TRANSMITTED);
	spi_register_callback(&spi_master_instance, SPI_read_callback, SPI_CALLBACK_BUFFER_RECEIVED);
//	spi_enable_callback(&spi_master_instance, SPI_CALLBACK_BUFFER_TRANSMITTED);
	spi_enable_callback(&spi_master_instance, SPI_CALLBACK_BUFFER_RECEIVED);
}

void SPI_init_samd(void)
{
	SPI_config_samd();
	SPI_config_callbacks();
}





// OBS£UGA PAMIÊCI FLASH

#if C_FLASH

void spi_samd_flash_clear(void)
{
	//wys³anie WREN w celu umo¿liwienia zapisu
	uint8_t instruction = SPIMEM_WRITE_ENABLE;
	spi_select_slave(&spi_master_instance, &slave_flash, true);				//zmiana chip select
	spi_write_buffer_wait(&spi_master_instance, &instruction, 1);			//wys³anie instrukcji WRITE ENABLE
	spi_select_slave(&spi_master_instance, &slave_flash, false);			//zmiana stanu chip select w celu zmiany stanu write enable latch
	
	//chip erase
	instruction = SPIMEM_ERASE_CHIP;
	spi_select_slave(&spi_master_instance, &slave_flash, true);	
	spi_write_buffer_wait(&spi_master_instance, &instruction, 1);
	spi_select_slave(&spi_master_instance, &slave_flash, false);
}

void spi_samd_flash_write(uint32_t address, uint8_t * data, uint8_t length)
{
	//wys³anie WREN w celu umo¿liwienia zapisu
	uint8_t instruction = SPIMEM_WRITE_ENABLE;
	spi_select_slave(&spi_master_instance, &slave_flash, true);				//zmiana CS
	spi_write_buffer_wait(&spi_master_instance, &instruction, 1);			//wys³anie instrukcji WRITE ENABLE
	spi_select_slave(&spi_master_instance, &slave_flash, false);			//zmiana stanu chip select w celu zmiany stanu write enable latch

	memmove(data+4, data, length);									//przesuniêcie danych, zrobienie miejsca na polecenie i adres
	
	//zbudowanie danych do wys³ania - instrukcja+adres+dane
	*data = SPIMEM_PAGE_PROGRAM;								//dodanie na pocz¹tku danych instrukcji zapisu danych
	*(data+1) = (uint8_t)(address >> 16);						//adres 24-bitowy
	*(data+2) = (uint8_t)(address >> 8);
	*(data+3) = (uint8_t)(address);
	length += 4;												//zapisanie d³ugoœci paczki danych razem z poleceniem i adresem
//	asm("BKPT");

	spi_select_slave(&spi_master_instance, &slave_flash, true);
	spi_write_buffer_wait(&spi_master_instance, data, length);
	spi_select_slave(&spi_master_instance, &slave_flash, false);
}

void spi_samd_flash_read(uint32_t address, uint8_t * byte_read)
{
	const uint8_t read_instruction[] = { SPIMEM_READ, (uint8_t)(address >> 16), (uint8_t)(address >> 8), (uint8_t)(address) };

	spi_select_slave(&spi_master_instance, &slave_flash, true);
	spi_write_buffer_wait(&spi_master_instance, read_instruction, sizeof(read_instruction));
	spi_read_buffer_wait(&spi_master_instance, byte_read, 1, 0);
	spi_select_slave(&spi_master_instance, &slave_flash, false);
}

void spi_samd_flash_dump(uint32_t start, uint32_t length, uint8_t * buffer)
{
	uint32_t i = start & 0xFFFFF0;							//zaokr¹glenie adresu do 16 bo i tak odczytujemy wielokrotnoœæ 16 bajtów
	
	const uint8_t read_instruction[] = { SPIMEM_READ, (uint8_t)(i >> 16), (uint8_t)(i >> 8), (uint8_t)(i) };
	spi_select_slave(&spi_master_instance, &slave_flash, true);			//ustawienie pinu chip select
	spi_write_buffer_wait(&spi_master_instance, read_instruction, sizeof(read_instruction));	//wys³anie instrukcji i adresu
	
	
	//odczytywanie odpowiedzi i obs³uga uarta
	uart_send_buffer((uint8_t *)"\n\rADDRESS  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");
	do
	{
		//odczyt 16 bajtów z pamiêci
		spi_read_buffer_wait(&spi_master_instance, buffer, 16, 0);
		
		uart_send_buffer((uint8_t *)"\r\n");
		
		//przes³anie przez uart w HEX
		//adres, separator, spacja
		uart_send_hex24(i, ':');
		uart_send_char(' ');
		
		//przes³anie kolejnych bajtów w HEX
		for (uint8_t h=0; h<=15; h++)
		{
			uart_send_hex(*(buffer+h), ' ');
		}
		
		//przes³anie kolejnych bajtów w ASCII
		for (uint8_t h=0; h<=15; h++)
		{
			if ((*(buffer+h) >= ' ') && (*(buffer+h) < 127)) uart_send_char(*(buffer+h));
			else uart_send_char(' ');
		}

 		i+=16;
	} while (i != 0 && i <= start+length-1);
	
	spi_select_slave(&spi_master_instance, &slave_flash, false);
}

void spi_samd_flash_erase(uint32_t address, uint8_t instruction)
{
	//write enable
	uint8_t wren = SPIMEM_WRITE_ENABLE;
	spi_select_slave(&spi_master_instance, &slave_flash, true);				//zmiana CS
	spi_write_buffer_wait(&spi_master_instance, &wren, 1);					//wys³anie instrukcji WRITE ENABLE
	spi_select_slave(&spi_master_instance, &slave_flash, false);			//zmiana stanu chip select w celu zmiany stanu write enable latch
		
	//sector/block erase
	const uint8_t erase_instruction[] = { instruction, (uint8_t)(address >> 16), (uint8_t)(address >> 8), (uint8_t)(address) };
	spi_select_slave(&spi_master_instance, &slave_flash, true);											//zmiana CS
	spi_write_buffer_wait(&spi_master_instance, erase_instruction, sizeof(erase_instruction));			//wys³anie instrukcji
	spi_select_slave(&spi_master_instance, &slave_flash, false);										//zmiana stanu chip select w celu zmiany stanu write enable latch
}

void spi_samd_flash_multipage(uint32_t address, uint8_t * data, uint8_t length)
{
	uint16_t AddressEnd = address + length - 1;					//adres ostatniego bajtu
	uint16_t PageStart = address / SPIMEM_PAGE_SIZE;			//nr pierwszej strony zapisywanych danych
	uint16_t PageEnd = AddressEnd / SPIMEM_PAGE_SIZE;			//nr ostatniej strony
	uint16_t ActualPage = PageStart;							//aktualna strona inicjowana jako startowa
	uint16_t ActualPageEnd;										//zmienna przechowuj¹ca ostatni adres aktualnej strony
	uint16_t ActualStart = address;								//pocz¹tek strony aktualnej
	uint16_t ActualEnd;											//adres koñcowy strony aktualnej
	uint16_t ActualLength;										//liczba danych pozosta³ych do zapisu

	// pêtla obejmuj¹ca wszystkie wyliczone strony
	while(ActualPage <= PageEnd)
	{
		// wyznaczenie adresu koñca, czy do koñca strony czy do koñca transakcji, jeœli koniec transakcji jest mniejszy ni¿ koniec strony
		ActualPageEnd = SPIMEM_PAGE_SIZE * (ActualPage+1) - 1;
		if(AddressEnd <= ActualPageEnd)
		{
			// koniec transakcji jest na tej stronie
			ActualEnd = AddressEnd;
		}
		else
		{
			// Koniec transakcji jest póŸniej
			ActualEnd = ActualPageEnd;
		}
		ActualLength = ActualEnd - ActualStart + 1;

		// zapis do pamiêci
		spi_samd_flash_write(ActualStart, data, ActualLength);
		data = data + ActualLength;

		// zwiêkszenie wskaŸników
		ActualPage++;
		ActualStart = ActualEnd + 1;
	}
}


#endif
