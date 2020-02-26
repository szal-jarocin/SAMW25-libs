#ifndef _SPI_SAMD_H_
#define _SPI_SAMD_H_

#include "stdint.h"

void SPI_init_samd(void);



#if C_FLASH
void	spi_samd_flash_write(uint32_t address, uint8_t * data, uint8_t length);
void	spi_samd_flash_read(uint32_t address, uint8_t * byte_read);
void	spi_samd_flash_dump(uint32_t start, uint32_t length, uint8_t * buffer);
void	spi_samd_flash_clear(void);
void	spi_samd_flash_erase(uint32_t address, uint8_t instruction);
void	spi_samd_flash_multipage(uint32_t address, uint8_t * data, uint8_t length);

#endif

#endif /* _SPI_SAMD_H_ */