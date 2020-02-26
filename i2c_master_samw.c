#include "i2c_master_samw.h"
#include "commands.h"
#include "interpreter.h"
#include "port.h"

#include <i2c_master_interrupt.h>
#include <string.h>

#define CONF_I2C_MASTER_MODULE    SERCOM0

struct i2c_master_packet wr_packet;
struct i2c_master_packet rd_packet;

/* Init software module instance. */
struct i2c_master_module i2c_master_instance;

uint8_t rd_buffer[DATA_LENGTH];						//bufor odebranych danych po MQTT
static uint8_t temp_register[1] = {0x05};					//rejestr termometru, z którego nastêpuje odczyt temperatury - zapisane jako pojedyncza zmienna uint8_t nie dzia³a³o
extern bool i2c_flag;

void i2c_read_complete_callback(struct i2c_master_module *const module)
{
	float temperature;										//do obliczania temperatury
	int temp1, temp2;										//czêœæ przed przecinkiem i po przecinku - printf nie dzia³a³ dla float ani double
	rd_buffer[0] = rd_buffer[0] & 0x1F;
	if ((rd_buffer[0] & 0x10) == 0x10)				//dla temperatury < 0 
	{
		rd_buffer[0] = rd_buffer[0] & 0x0F;
		temperature = 256 - (rd_buffer[0] * 16 + rd_buffer[1]/16);
	}
	
	temperature = ((float)rd_buffer[0] * 16 + (float)rd_buffer[1]/16);
	temp1 = (int)temperature;
	temp2 = (int)(temperature*10000 - temp1*10000);
	
	
	char string[12];
	sprintf(string, "%i.%i", temp1, temp2);
	uart_buffer_clear();
	uart_add_to_buffer(string);
	send_data();
//	printf("%i.%i\r\n", temp1, temp2);
// 	xor_checksum_count((char *)temp1);
// 	xor_checksum_count(".");
// 	xor_checksum_count((char *)temp2);
//	xor_checksum_send("\r\n");
	memset(&rd_buffer, 0, DATA_LENGTH);
	i2c_deinit(0, 0);
	i2c_flag = false;
}

//konfiguracja i2c
void i2c_config(void)
{
	#ifndef _i2c_config_
	#define _i2c_config_
		struct system_pinmux_config i2c_pinmux;
		system_pinmux_get_config_defaults(&i2c_pinmux);
		i2c_pinmux.mux_position = 2;						//konfiguracja C - SERCOM0
		i2c_pinmux.input_pull = SYSTEM_PINMUX_PIN_PULL_NONE;
		i2c_pinmux.direction = SYSTEM_PINMUX_PIN_DIR_OUTPUT_WITH_READBACK;
		system_pinmux_pin_set_config(SDA_PIN, &i2c_pinmux);
		port_pin_set_output_level(SDA_PIN, false);
		
		i2c_pinmux.direction = SYSTEM_PINMUX_PIN_DIR_OUTPUT;
		system_pinmux_pin_set_config(SCL_PIN, &i2c_pinmux);
		
	
		/* Initialize config structure and software module */
		struct i2c_master_config config_i2c_master;
		
		i2c_master_get_config_defaults(&config_i2c_master);
		
		/* Change buffer timeout to something longer */
		config_i2c_master.buffer_timeout = 65535;
		config_i2c_master.pinmux_pad0	= SDA_PIN;
		config_i2c_master.pinmux_pad1	= SCL_PIN;
		
		
		/* Initialize and enable device with config */
		i2c_master_init(&i2c_master_instance, CONF_I2C_MASTER_MODULE, &config_i2c_master);
	#endif // _i2c_config_
	i2c_master_enable(&i2c_master_instance);
}

//konfiguracja funkcji callback
void i2c_config_callback(void)
{
	#ifndef _i2c_config_callback_
	#define _i2c_config_callback_
		/* Register callback function. */
		i2c_master_register_callback(&i2c_master_instance, i2c_read_complete_callback, I2C_MASTER_CALLBACK_READ_COMPLETE);
	#endif	// _i2c_config_callback_

	i2c_master_enable_callback(&i2c_master_instance, I2C_MASTER_CALLBACK_READ_COMPLETE);
	//konfiguracja struktury odbieranych danych
	rd_packet.address		= I2C_THERMO_ADDRESS;
	rd_packet.data_length	= DATA_LENGTH;
	rd_packet.data			= rd_buffer;
}

//wy³¹czenie komunikacji po I2C
void i2c_disable(void)
{
	i2c_master_disable_callback(&i2c_master_instance, I2C_MASTER_CALLBACK_READ_COMPLETE);
	i2c_master_reset(&i2c_master_instance);															//z samym disable nie mo¿na by³o uruchomiæ atwina po SPI
	//i2c_master_disable(&i2c_master_instance);
}

//wys³anie paczki danych
uint8_t I2C_Write(uint16_t address, uint8_t * data, uint16_t length)
{
	wr_packet.address		= address;
	wr_packet.data			= data;
	wr_packet.data_length	= length;
	return(i2c_master_write_packet_wait(&i2c_master_instance, &wr_packet));
}

//odbiór paczki danych
void I2C_Read(uint16_t address, uint8_t * buf, uint16_t length)
{
	memset(rd_buffer, 0, sizeof(rd_buffer));
	rd_packet.address		= address;
	rd_packet.data			= buf;
	rd_packet.data_length	= length;
	i2c_master_read_packet_wait(&i2c_master_instance, &rd_packet);
//	asm("BKPT");
}

uint8_t I2C_Start(uint16_t address)
{
	enum status_code scan_status;
	
	
	_i2c_master_wait_for_sync(&i2c_master_instance);										//synchronizacja zegara i2c
	I2C_Stop();
	i2c_master_instance.hw->I2CM.ADDR.reg = address | (0 << SERCOM_I2CM_ADDR_HS_Pos);		//ustawienie adresu
	scan_status = _i2c_master_wait_for_bus(&i2c_master_instance);							//sprawdzenie czy magistrala nie jest zajêta
	if(scan_status == STATUS_OK) _i2c_master_address_response(&i2c_master_instance);
	return scan_status;
}

void I2C_Stop(void)
{
	i2c_master_instance.send_nack = true;
	i2c_master_instance.send_stop = true;
	i2c_master_send_nack(&i2c_master_instance);							//wys³anie NAK i stopu
	i2c_master_send_stop(&i2c_master_instance);
}

//odczyt temperatury z termometru AVR IoT
void i2c_read_temp(void)
{
	wr_packet.address		= I2C_THERMO_ADDRESS;						//adres termometru AVT IoT
	wr_packet.data_length	= 1;
	wr_packet.data			= temp_register;
	i2c_master_write_packet_wait(&i2c_master_instance, &wr_packet);		//polecenie odes³ania rejestru temperatury
	i2c_master_read_packet_job(&i2c_master_instance, &rd_packet);		//odczyt przes³anego z termometru rejestru temperatury
}

//funkcja skanuj¹ca dostêpne urz¹dzenia po magistrali I2C
void i2c_scan(void)
{
	enum status_code scan_status;								//zmienna przechowuj¹ca kod b³êdu
	uint8_t i=0;												//kolejne adresy I2C
	
	#if I2C_MASTER_CALLBACK_MODE == true
	/* Check if the I2C module is busy with a job */
	if (i2c_master_instance.buffer_remaining > 0) scan_status = STATUS_BUSY;
	else scan_status = STATUS_OK;
	#endif
	
	//je¿eli I2C nie jest zajête
	if (scan_status == STATUS_OK)
	{
		uart_buffer_clear();
		i2c_master_instance.send_stop = true;
		i2c_master_instance.send_nack = true;
		SercomI2cm *const i2c_module = &(i2c_master_instance.hw->I2CM);
		
		
		_i2c_master_wait_for_sync(&i2c_master_instance);						//synchronizacja I2C
		while(1) {
			I2C_Stop();
			i2c_module->ADDR.reg = (i << 1) | I2C_TRANSFER_READ | (0 << SERCOM_I2CM_ADDR_HS_Pos);					//ustawienie kolejnego adresu
			scan_status = _i2c_master_wait_for_bus(&i2c_master_instance);											//sprawdzenie, czy magistrala jest wolna
			if (scan_status == STATUS_OK) scan_status = _i2c_master_address_response(&i2c_master_instance);			//czekanie na odpowiedŸ z podanego adresu
			if (scan_status == STATUS_OK)
			{
				char string[3];
				sprintf(string, "%02X ", i);					//wyœwietlenie adresu dostêpnego urz¹dzenia
				uart_add_to_buffer(string);
			}
			if (i == 255)
			{
				I2C_Stop();
				send_data();
				//printf("\r\n");
				return;		//koniec skanowania
			}
			i++;
		}
	}
}
