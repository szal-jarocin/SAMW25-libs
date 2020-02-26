#ifndef _INTERPRETER_H_
#define _INTERPRETER_H_

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "usart.h"
#include "usart_interrupt.h"
#include "ASCII_definitions.h"


#define	MAX_RX_BUFFER_LENGTH	256
#define MAX_SERIALS_CHAR_COUNT	1460
#define MAX_ARGS				135
#define MAX_SERIALS_LENGTH		50


//struktura bufora uarta
struct buffer_struct {
	uint16_t received_count;									//numer wskazywanego znaku
	uint8_t buffer[MAX_RX_BUFFER_LENGTH];						//bufor
	void	(*data_received_callback)(uint8_t *buffer);
};

struct serials_struct {
	uint16_t received_count;									//numer wskazywanego znaku
	uint8_t buffer[MAX_SERIALS_CHAR_COUNT];						//bufor
	};



struct commands_struct {
	const char *command_name;									//string do wpisania w konsolê
	void (*func)(uint8_t argc, uint8_t * argv[]);				//wskaŸnik do funkcji wykonywanej po wpisaniu polecenia
};


void uart_send_buffer(const uint8_t *data);
void uart_send_hex_string(const uint8_t * String, const uint16_t Length, const uint8_t Separator, const uint8_t BytesInRow);
void uart_send_hex(const uint8_t data, const uint8_t separator);
void uart_send_hex16(const uint16_t data, const uint8_t separator);
void uart_send_hex24(const uint32_t data, const uint8_t separator);
void uart_send_dec(uint32_t Value);
void uart_send_char(char character);
void uart_buffer_clear(void);
void uart_buffer_last_clear(void);
void uart_add_to_buffer(const char * data);
uint8_t xor_calculate(const char * data);
uint16_t crc_calculate(uint8_t * Data, uint8_t Length);
void send_data(void);
void send_data_loop(uint8_t argc, uint8_t * argv[]);
void send_command_check(const char * command);
//void send_commands_loop_check(const char * command, uint8_t argc, uint8_t * argv[], bool disable_timer, bool enable_timer);
void send_commands_loop_check(const char * command, uint8_t argc, uint8_t * argv[]);
void command_split(uint8_t *buf, uint8_t *argc, uint8_t *argv[]);
void command_compare(void);
enum Console_Res_t usart_receive(void);
enum CmdRes_t parse_char_to_hex(const uint8_t * InputChar, uint8_t * OutputChar);
enum CmdRes_t parse_dec16(const uint8_t * Argument, uint16_t * Output, const uint16_t MaxValue);
void parse_HEX8(const uint8_t * input, uint8_t * output);
void parse_HEX8_int(const uint8_t * input, int8_t * output);
void parse_HEX16(const uint8_t * input, uint16_t * output);
void parse_HEX24(const uint8_t * input, uint32_t * output);
void parse_HEX32(const uint8_t * input, uint32_t * output);
enum CmdRes_t Parse_HexString(const uint8_t * InputString, uint8_t * OutputString, uint8_t * OutputLength, const uint8_t MaxLength, const uint8_t MinLength);
void Base64decode(uint8_t OutputBin[], uint8_t * InputBase64, uint16_t InputLength, uint16_t * OutputLength);
void Base64encode(uint8_t * OutputBase64, const uint8_t * InputBin, uint8_t InputLength);

#endif /* _INTERPRETER_H_ */