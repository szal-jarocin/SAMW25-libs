#include "timer_samw.h"
#include "samw25_xplained_pro.h"
#include "interpreter.h"
#include <port.h>
#include <tc_interrupt.h>
#include <usart_interrupt.h>
#include <delay.h>
#include "interpreter.h"
#include "main.h"

volatile enum Console_Res_t uart_state = Console_OK;
volatile uint8_t tick30ms = 0;
volatile uint16_t tick1ms = 0;

extern uint16_t received_char;
extern struct usart_module cdc_uart_module;

//callback uzywany do odczytu znaków z uart - gdy wywo³ywane w mainie MQTT stwarza³o problemy (odczyt tylko pierwszych 3 znaków)
void tc_callback_read_usart(struct tc_module *const module_inst)
{
	received_char = 0;
	enum status_code ret = usart_read_wait(&cdc_uart_module, &received_char);
	if (STATUS_OK == ret)
	{
		uart_state = usart_receive();
	}
}

//callback u¿ywany do inkrementacji timeoutu - co ok. 100 ms (3 ticki) ponowne wysy³anie polecenia
void tc_callback_increment_timeout(struct tc_module *const module_inst)
{
	tick30ms++;
}

//callback u¿ywany do cyklicznego wysy³ania zapytañ do AWSa
void tc_callback_aws(struct tc_module *const module_inst)
{
	//wywo³anie funkcji przerabiaj¹cej odebrane dane
}

//callback u¿ywany do pomiaru czasu dzia³ania funkcji
void tc_callback_measure(struct tc_module *const module_inst)
{
	tick1ms++;
}

//konfiguracja timera
void configure_timer_uart(void)
{
	struct tc_config config_tc_uart;

	tc_get_config_defaults(&config_tc_uart);
	config_tc_uart.counter_size = TC_COUNTER_SIZE_8BIT;
// 	config_tc.clock_source = GCLK_GENERATOR_0;						//zegar 48 MHz
// 	config_tc.clock_prescaler = TC_CLOCK_PRESCALER_DIV16;			//czêstotliwoœæ ok. 115384
// 	config_tc.counter_8_bit.period = 26;
	
	config_tc_uart.clock_source = GCLK_GENERATOR_0;						//zegar 48 MHz
	config_tc_uart.clock_prescaler = TC_CLOCK_PRESCALER_DIV64;			//czêstotliwoœæ 57692
	config_tc_uart.counter_8_bit.period = 12;

	tc_init(&tc_instance_uart, CONF_TC_UART, &config_tc_uart);
	tc_enable(&tc_instance_uart);
}

void configure_timer_timeout(void)
{
	struct tc_config config_tc_timeout;
	tc_get_config_defaults(&config_tc_timeout);
	config_tc_timeout.counter_size = TC_COUNTER_SIZE_8BIT;
	config_tc_timeout.clock_source = GCLK_GENERATOR_3;						//OSC32K, OPIS W conf_clocks.h
	config_tc_timeout.clock_prescaler = TC_CLOCK_PRESCALER_DIV1024;		//32 Hz (31,25 ms)
//	config_tc_timeout.counter_8_bit.period = 10;
	// 	config_tc_timeout.clock_prescaler = TC_CLOCK_PRESCALER_DIV64;			//10 Hz - przerwanie co 100 ms
	// 	config_tc_timeout.counter_8_bit.period = 50;

	tc_init(&tc_instance_timeout, CONF_TC_TIMEOUT, &config_tc_timeout);
	tc_enable(&tc_instance_timeout);
}

void configure_timer_AWS(void)
{
	struct tc_config config_tc_aws;
	tc_get_config_defaults(&config_tc_aws);
	config_tc_aws.counter_size = TC_COUNTER_SIZE_8BIT;
	config_tc_aws.clock_source = GCLK_GENERATOR_3;
	config_tc_aws.clock_prescaler = TC_CLOCK_PRESCALER_DIV1024;
	config_tc_aws.counter_8_bit.period = 160;							//przerwanie co 5 s
	//tc_init(&tc_instance_aws, CONF_TC_AWS, &config_tc_aws);
}

void configure_timer_measure(void)
{
	struct tc_config config_tc_measure;
	tc_get_config_defaults(&config_tc_measure);
	config_tc_measure.counter_size = TC_COUNTER_SIZE_8BIT;
	config_tc_measure.clock_source = GCLK_GENERATOR_3;
	config_tc_measure.clock_prescaler = TC_CLOCK_PRESCALER_DIV16;
	config_tc_measure.counter_8_bit.period = 2;							//przerwanie co 1 ms
	tc_init(&tc_instance_measure, CONF_TC_MEASURE, &config_tc_measure);
}

void timer_AWS_enable(void)
{
	tc_enable(&tc_instance_timeout);
}

void timer_AWS_disable(void)
{
	tc_disable(&tc_instance_aws);
}

void timer_measure_enable(void)
{
	tc_enable(&tc_instance_measure);
}

void timer_measure_disable(void)
{
	tc_disable(&tc_instance_measure);
}

//konfiguracja callbacka
void configure_tc_callbacks(void)
{
	tc_register_callback(&tc_instance_uart, tc_callback_read_usart, TC_CALLBACK_OVERFLOW);
	tc_enable_callback(&tc_instance_uart, TC_CALLBACK_OVERFLOW);
 	tc_register_callback(&tc_instance_timeout, tc_callback_increment_timeout, TC_CALLBACK_OVERFLOW);
 	tc_enable_callback(&tc_instance_timeout, TC_CALLBACK_OVERFLOW);
//	tc_register_callback(&tc_instance_aws, tc_callback_aws, TC_CALLBACK_OVERFLOW);
//	tc_enable_callback(&tc_instance_aws, TC_CALLBACK_OVERFLOW);
	tc_register_callback(&tc_instance_measure, tc_callback_measure, TC_CALLBACK_OVERFLOW);
	tc_enable_callback(&tc_instance_measure, TC_CALLBACK_OVERFLOW);
}

//wywo³anie w mainie, konfiguracja timera i callbacka
void timer_init(void)
{
	configure_timer_uart();
	configure_timer_timeout();
	configure_timer_measure();
	configure_tc_callbacks();
}

//wy³¹czenie timera
void timer_disable(void)
{
	tc_disable_callback(&tc_instance_uart, TC_CALLBACK_OVERFLOW);
	tc_unregister_callback(&tc_instance_uart, TC_CALLBACK_OVERFLOW);
	tc_disable(&tc_instance_uart);
}
