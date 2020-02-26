#ifndef _ATWINC_FLASH_H_
#define _ATWINC_FLASH_H_

#include <stdint.h>

void cert_read(void);
void cert_write(void);
void provision_page_read(void);
void provision_page_default(void);
void root_cert_default(void);
void tls_lib_test(void);
void atwinc_write_bridge_callback(uint8_t *buffer, uint16_t count);

#endif /* _ATWINC_FLASH_H_ */