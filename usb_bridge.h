#ifndef _USB_BRIDGE_H_
#define _USB_BRIDGE_H_


void usb_rx_notify(uint8_t port);

/*! \brief Configures communication line
 *
 * \param cfg      line configuration
 */
//void usb_config(uint8_t port, usb_cdc_line_coding_t * cfg);

/*! \brief Opens communication line
 */
void usb_open(uint8_t port);

/*! \brief Closes communication line
 */
void usb_close(uint8_t port);
bool usb_cdc_enable(uint8_t port);
void usb_cdc_disable(uint8_t port);
void usb_cdc_set_dtr(uint8_t port, bool b_enable);
void usart_transmitted(void);
void usb_transmit(const char * string);

#endif /* _USB_BRIDGE_H_ */