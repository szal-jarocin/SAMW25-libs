#ifndef _I2C_MASTER_SAMW_H_
#define _I2C_MASTER_SAMW_H_

#include <i2c_master.h>


#define I2C_THERMO_ADDRESS		0x18
#define DATA_LENGTH				32

#define SDA_PIN PIN_PA08
#define SCL_PIN PIN_PA09

extern enum status_code _i2c_master_wait_for_bus(struct i2c_master_module *const module);
extern enum status_code _i2c_master_address_response(struct i2c_master_module *const module);

void i2c_read_complete_callback(struct i2c_master_module *const module);
uint8_t I2C_Start(uint16_t address);
void I2C_Stop(void);
uint8_t I2C_Write(uint16_t address, uint8_t * data, uint16_t length);
void I2C_Read(uint16_t address, uint8_t * buf, uint16_t length);
void i2c_config(void);
void i2c_config_callback(void);
void i2c_disable(void);
void i2c_main_handler(void);
void i2c_read_temp(void);
void i2c_scan(void);

#endif /* _I2C_MASTER_SAMW_H_ */