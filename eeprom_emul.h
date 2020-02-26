#ifndef _EEPROM_EMUL_H_
#define _EEPROM_EMUL_H_

#define SERIAL_ADDRESS		0
#define SSID_ADDRESS		20
#define PASSWORD_ADDRESS	60

#define SERIAL_LENGTH		18
#define SSID_LENGTH			32
#define PASSWORD_LENGTH		64

void configure_eeprom(void);
void EEPROM_Read(uint8_t Address, uint8_t Data[], uint8_t length);
void EEPROM_Write(uint8_t Address, uint8_t *Data, uint8_t length);
void EEPROM_emul_config(void);
void EEPROM_save_defaults(void);
void factory_reset(void);


#endif // _EEPROM_EMUL_H_

