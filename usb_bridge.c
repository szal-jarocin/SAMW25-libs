#include <asf.h>
#include "usb_bridge.h"
#include "usart_interrupt.h"
#include "interpreter.h"
#include "main.h"

bool tx_callback_flag = 0;

volatile bool main_b_cdc_enable = false;

extern uint16_t received_char;
extern volatile enum Console_Res_t uart_state;
extern struct usb_module usb_device;

void usb_rx_notify(uint8_t port)
{
	while(udi_cdc_is_rx_ready())		//pêtla wykonywana dopóki s¹ do odbioru jeszcze jakieœ znaki
	{
		received_char = udi_cdc_getc();
		uart_state = usart_receive();
	}
}

bool usb_cdc_enable(uint8_t port)					//wejœcie z funkcji obs³ugi przerwañ USB (_usb_device_interrupt_handler)
{
	main_b_cdc_enable = true;						//inicjacja funkcji CDC (urz¹dzenie USB jako port COM) i rozpoczêcie komunikacji
	// Open communication
//	uart_open(port);
	return true;
}

void usb_cdc_disable(uint8_t port)
{
	main_b_cdc_enable = false;
	// Close communication
//	uart_close(port);
}

void usb_cdc_set_dtr(uint8_t port, bool b_enable)
{
// 	if (b_enable) {
// 		// Host terminal has open COM
// 		ui_com_open(port);
// 		}else{
// 		// Host terminal has close COM
// 		ui_com_close(port);
// 	}
}

void usb_transmit(const char * string)
{
	udi_cdc_write_buf(string, strlen(string));
//	usb_device_endpoint_write_buffer_job(&usb_device, 0, (uint8_t *)string, strlen(string));
}