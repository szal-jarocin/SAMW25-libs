#ifndef _SPI_MEM_H_
#define _SPI_MEM_H_

#if C_FLASH

// komendy dla pamiêci flash GD25Q64C
#define		SPIMEM_READ						0x03		// Read data from memory array beginning at selected address
#define		SPIMEM_PAGE_PROGRAM				0x02		// Write data to memory array beginning at selected address
#define		SPIMEM_WRITE_ENABLE				0x06		// Set the write enable latch (enable write operations)
#define		SPIMEM_WRITE_DISABLE			0x04		// Reset the write enable latch (disable write operations)
#define		SPIMEM_STATUS_READ_1			0x05		// Read STATUS register
#define		SPIMEM_STATUS_READ_2			0x35
#define		SPIMEM_STATUS_READ_3			0x15
#define		SPIMEM_STATUS_WRITE_1			0x01		// Write STATUS register
#define		SPIMEM_STATUS_WRITE_2			0x31
#define		SPIMEM_STATUS_WRITE_3			0x11
#define		SPIMEM_ERASE_SECTOR				0x20		// Sector Erase – erase one sector in memory array
#define		SPIMEM_ERASE_BLOCK32			0x52
#define		SPIMEM_ERASE_BLOCK64			0xD8
#define		SPIMEM_ERASE_CHIP				0xC7		// Chip Erase – erase all sectors in memory array
#define		SPIMEM_WAKE						0xAB		// Release from Deep power-down and read electronic signature
#define		SPIMEM_SLEEP					0xB9		// Deep Power-Down mode

#define		SPIMEM_PAGE_SIZE				256

//#define		SPIMEM_ERASE_PAGE				0x42		// Page Erase – erase one page in memory array


#endif

#endif /* _SPI_MEM_H_ */