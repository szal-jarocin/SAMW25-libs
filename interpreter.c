#include <string.h>
#include <stdio_serial.h>
#include <delay.h>
#include "main.h"
#include "interpreter.h"
#include "ASCII_definitions.h"
#include "commands_list.h"
#include "port.h"
#include "samw25_xplained_pro.h"
#include "timer_samw.h"
#include "eeprom_emul.h"
#include "udi_cdc.h"

extern volatile enum Console_Res_t uart_state;					//volatile bo inaczej optymalizator obcina³ zmienn¹
extern volatile bool list_get_flag;

struct buffer_struct buffer_usart;
struct buffer_struct buffer_usart_last;
struct buffer_struct buffer_response;

//struct buffer_struct serials_list;
struct usart_module cdc_uart_module;

extern bool uart_flag;
extern bool list_start_flag;
extern bool cert_write_flag;
extern bool main_b_cdc_enable;
extern volatile uint8_t tick30ms;

static inline bool compare_string(const char *string1, const char* string2);
void (*command_pointer)(uint8_t argc, uint8_t *argv[]);

uint16_t received_char;
uint16_t crc_interpreter = 0;			//zmienna przechowuj¹ca obliczon¹ sumê kontroln¹
uint8_t crc_H_interpreter = 0;
uint8_t crc_L_interpreter = 0;
uint8_t token_read = 0;
uint8_t token_send = 0;

char last_card_number[20];

bool wifi_name_flag = false;			//flaga wystawiana, kiedy w argumencie pojawi siê cudzys³ów - wtedy spacja nie dzieli argumentów ale dodawana jest do stringa
bool checksum_flag = false;				//flaga wystawiana po odebraniu ETB - nastêpny znak to wartoœæ sumy kontrolnej wiêc musi zostaæ pobrany bez wzglêdu na wartoœæ
bool checksum_error = false;			//flaga wystawiana, kiedy nie zgadzaj¹ siê sumy kontrolne obliczona i odebrana
bool send_command_flag = false;			//flaga wystawiana po rozpoczêciu wysy³ki polecenia, usuwana przy odbiorze ACK, jeœli flaga wystawiona to interpreter odbiera tylko ACK lub NAK
bool return_flag = false;				//flaga wystawiana po odebraniu SOH, dopiero po odebraniu SOH CR (jednokrotnie) dzia³a jako potwierdzenie polecenia, inaczej ignorowane
bool received_command_flag = false;

//wys³anie bufora odpowiedzi przez UART
void uart_send_buffer(const uint8_t *data)
{
	usart_write_buffer_wait(&cdc_uart_module, data, strlen((char *)data));
	#if C_USB
		if(main_b_cdc_enable) usb_transmit((char *)data);							//warunek sprawdzaj¹cy czy urz¹dzenie zosta³o pod³¹czone do mastera nako port COM
	#endif
}

// Ci¹g znaków prezentowany jako HEX
void uart_send_hex_string(const uint8_t * String, const uint16_t Length, const uint8_t Separator, const uint8_t BytesInRow)
{
	uart_send_char(CR);
	for(uint16_t i=0; i<Length; i++) {
		
		// Przejœcie do nowej linii (nie dotyczny pierwszego wyœwietlanego znaku)
		if((i%BytesInRow == 0) && i != 0) {
			uart_send_char(CR);
		}

		// Wyœwietlenie znaku
		uart_send_hex(*(String+i), Separator);
	}
	uart_send_char(CR);
}

void uart_send_hex(const uint8_t data, const uint8_t separator)
{
	uint8_t nibbleH = (data & 0xF0) >> 4;
	uint8_t nibbleL = (data & 0x0F);
	
	if (nibbleH <= 9) uart_send_char(nibbleH + '0');
	else uart_send_char(nibbleH + 55);
	
	if (nibbleL <= 9) uart_send_char(nibbleL + '0');
	else uart_send_char(nibbleL + 55);
	
	if (separator) uart_send_char(separator);
}

void uart_send_hex16(const uint16_t data, const uint8_t separator)
{
	uart_send_hex((uint8_t)((data & 0xFF00) >> 8), 0);
	uart_send_hex((uint8_t)((data & 0x00FF)		), separator);
}

void uart_send_hex24(const uint32_t data, const uint8_t separator)
{
	uart_send_hex((uint8_t)((data & 0x00FF0000) >> 16), 0);
	uart_send_hex((uint8_t)((data & 0x0000FF00) >> 8), 0);
	uart_send_hex((uint8_t)((data & 0x000000FF)		), separator);
}

void uart_send_dec(uint32_t Value)
{
	if(Value==0) {
		uart_send_char('0');
		return;
	}
	
	uint8_t cyfra[10];
	memset(cyfra, 0, sizeof(cyfra));
	int8_t i=0;
	
	while(Value) {
		cyfra[i] = (uint8_t)(Value%10);
		Value = Value / 10;
		++i;
	}
	
	while(i--) {
		uart_send_char(cyfra[i]+48);
	}
}

void uart_send_char(char character)
{
	printf("%c", character);
	#if C_USB
		if(main_b_cdc_enable) udi_cdc_write_buf(&character, 1);
	#endif
}

//czyszczenie bufora UARTa
void uart_buffer_clear(void)
{
	memset(buffer_usart.buffer, 0, MAX_RX_BUFFER_LENGTH);
	buffer_usart.received_count = 0;
}

//czyszczenie bufora poprzedniego poleceniea odebranego przez UART
void uart_buffer_last_clear(void)
{
	memset(buffer_usart_last.buffer, 0, MAX_RX_BUFFER_LENGTH);
	buffer_usart_last.received_count = 0;
}

//dodanie danych do bufora UART
void uart_add_to_buffer(const char * data)
{
	const char * data_pointer = data;
	while (*data_pointer != 0)
	{
		buffer_usart.buffer[buffer_usart.received_count++] = *data_pointer;
		data_pointer++;
	}
}

//obliczenie "sumy kontrolnej" xor
uint8_t xor_calculate(const char * data)
{
	char * data_pointer = (char *)data;
	uint8_t xor_value = 0;
	while (*data_pointer != 0)
	{
		xor_value ^= *data_pointer;
		data_pointer++;
	}
	return xor_value;
}

//obliczenie sumy kontrolnej CRC
uint16_t crc_calculate(uint8_t * Data, uint8_t Length)
{
	uint16_t CRC = 0x6363;
	
	while(Length--)
	{
		uint8_t Byte = *Data++;
		Byte = Byte ^ (uint8_t)CRC;
		Byte = Byte ^ (Byte << 4);
		CRC = (CRC >> 8) ^ ((uint16_t)Byte << 8) ^ ((uint16_t)Byte << 3) ^ (Byte >> 4);
	}
	
	return CRC;
}

//dokoñczenie budowy paczki danych UART zgodnie z protoko³em komunikacyjnym SAFLINK
void send_data(void)
{
	crc_interpreter = crc_calculate(buffer_usart.buffer, strlen((char *)buffer_usart.buffer));

	buffer_usart.buffer[buffer_usart.received_count++] = US;									//dodanie znaku US jako znacznik sumy kontrolnej
	buffer_usart.buffer[buffer_usart.received_count++] = (uint8_t)(crc_interpreter >> 8);
	buffer_usart.buffer[buffer_usart.received_count++] = (uint8_t)(crc_interpreter);			//suma kontrolna
	buffer_usart.buffer[buffer_usart.received_count++] = token_read;							//token - licznik otrzymanego polecenia w celu identyfikacji odpowiedzi
	
	
	usart_write_buffer_wait(&cdc_uart_module, buffer_usart.buffer, buffer_usart.received_count);			//zamiast printf ¿eby nie pomin¹æ ewentualnego NULLa w sumie CRC
	
	#if C_USB
		if(main_b_cdc_enable) udi_cdc_write_buf(buffer_usart.buffer, buffer_usart.received_count);
	#endif
	
	buffer_response = buffer_usart;
// 	uint8_t tries_counter = 0;
// 	tick30ms = 0;
// 	while (uart_state != Console_ReceivedACK)
// 	{
// 		if (tick30ms > 2)
// 		{
// 			tick30ms = 0;
// 			usart_write_buffer_wait(&cdc_uart_module, buffer_usart.buffer, buffer_usart.received_count);
// 			tries_counter++;
// 		}
// 		if (tries_counter >= 10)
// 		{
// 			printf("FATAL_ERROR");
// 			//b³¹d transmisji - zatrzymanie programu, czekanie na reset
// 			while(1);
// 		}
// 	}
}

//wysy³anie wielu odpowiedzi jedna po drugiej (aktualnie nie jest u¿ywane)
void send_data_loop(uint8_t argc, uint8_t * argv[])
{
	send_command_flag = true;
	for (int i=0; i<argc; i++)
	{
		
		if (argv[i] != 0)
		{
			while (command_pointer != list_get_command);
			command_pointer = list_download_command;
			
			uart_buffer_clear();
			char string[25];
			memset(string, 0, 25);
			crc_interpreter = crc_calculate(argv[i], strlen((char *)argv[i]));
			
			sprintf(string, "%s%c", argv[i], US);
			uart_add_to_buffer(string);
			buffer_usart.buffer[buffer_usart.received_count++] = (uint8_t)(crc_interpreter >> 8);
			buffer_usart.buffer[buffer_usart.received_count++] = (uint8_t)(crc_interpreter);
			buffer_usart.buffer[buffer_usart.received_count++] = token_read;

			usart_write_buffer_wait(&cdc_uart_module, buffer_usart.buffer, buffer_usart.received_count);
		}
	}
}

//wysy³anie kilku poleceñ jedno po drugim z ponownym wys³aniem przy braku odpowiedzi
void send_commands_loop_check(const char * command, uint8_t argc, uint8_t * argv[])
{
	for (int i=0; i<argc; i++)
	{
		if (argv[i] != 0)
		{
			send_command_flag = true;
			uart_buffer_clear();
			char string[40];
			sprintf(string, "%s %s", command, (char *)argv[i]);
			crc_interpreter = xor_calculate((char *)string);
			
			sprintf(string, "%c%s %s%c", DC1, command, argv[i], US);		//US - znacznik sumy kontrolnej
			uart_add_to_buffer(string);
			buffer_usart.buffer[buffer_usart.received_count++] = (uint8_t)(crc_interpreter >> 8);
			buffer_usart.buffer[buffer_usart.received_count++] = (uint8_t)(crc_interpreter);			//suma kontrolna
			buffer_usart.buffer[buffer_usart.received_count++] = token_send;							//licznik wys³anych poleceñ
			
			usart_write_buffer_wait(&cdc_uart_module, buffer_usart.buffer, buffer_usart.received_count);
//			buffer_usart.buffer[0] = DC2;
			uint8_t tries_counter = 0;
			tick30ms = 0;
			while (uart_state != Console_ReceivedACK)
			{
				if (tick30ms > 2)
				{
					tick30ms = 0;
					usart_write_buffer_wait(&cdc_uart_module, buffer_usart.buffer, buffer_usart.received_count);		//ponowne wys³anie polecenia
					tries_counter++;
				}
				if (tries_counter >= 10)
				{
					usart_write_buffer_wait(&cdc_uart_module, (uint8_t *)"FATAL_ERROR", strlen("FATAL_ERROR"));
					//printf("FATAL_ERROR");				//b³¹d transmisji, zawieszenie programu w przypadku braku odpowiedzi, AVR przy timeoucie zresetuje modu³
					while(1);
				}
			}
		}
	}
}

//wys³anie polecenia ze sprawdzeniem odpowiedzi i ponownym nadaniem przy jej braku
void send_command_check(const char * command)										//mo¿na daæ dodatkowe argumenty dla startu i stopu timera
{
	send_command_flag = true;
	uart_buffer_clear();
	uint8_t length = strlen(command);
	crc_interpreter = crc_calculate((uint8_t *)command, length);
	char string[50];
	sprintf(string, "%c%s%c", DC1, command, US);						//DC1 - znacznik polecenia, US - znacznik sumy kontrolnej
	uart_add_to_buffer(string);
	buffer_usart.buffer[buffer_usart.received_count++] = (uint8_t)(crc_interpreter >> 8);
	buffer_usart.buffer[buffer_usart.received_count++] = (uint8_t)(crc_interpreter);			//suma kontrolna
	buffer_usart.buffer[buffer_usart.received_count++] = token_send;							//licznik wys³anego polecenia
	
	usart_write_buffer_wait(&cdc_uart_module, buffer_usart.buffer, buffer_usart.received_count);
	
	#if C_USB
		if(main_b_cdc_enable) udi_cdc_write_buf(buffer_usart.buffer, buffer_usart.received_count);
	#endif
	
//	buffer_usart.buffer[0] = DC2;
	uint8_t tries_counter = 0;
	tick30ms = 0;
	while (uart_state != Console_ReceivedACK)
	{
		if (tick30ms > 2)
		{
			tick30ms = 0;
			usart_write_buffer_wait(&cdc_uart_module, buffer_usart.buffer, buffer_usart.received_count);		//ponowne wys³anie polecenia
			
			#if C_USB
				if(main_b_cdc_enable) udi_cdc_write_buf(buffer_usart.buffer, buffer_usart.received_count);
			#endif
			
			tries_counter++;
		}
		if (tries_counter >= 10)
		{
			printf("FATAL_ERROR");										//b³¹d transmisji, zawieszenie programu, przy timeoucie AVR zresetuje modu³
			#if C_USB
				if(main_b_cdc_enable) usb_transmit("FATAL_ERROR");
			#endif
			while(1);
		}
	}
	//token_send++
}

//konfiguracja UART z biblioteki ASF
void configure_console(void)
{
	struct usart_config usart_conf;

	usart_get_config_defaults(&usart_conf);
	usart_conf.mux_setting = EDBG_CDC_SERCOM_MUX_SETTING;
	usart_conf.pinmux_pad0 = EDBG_CDC_SERCOM_PINMUX_PAD0;
	usart_conf.pinmux_pad1 = EDBG_CDC_SERCOM_PINMUX_PAD1;
	usart_conf.pinmux_pad2 = EDBG_CDC_SERCOM_PINMUX_PAD2;
	usart_conf.pinmux_pad3 = EDBG_CDC_SERCOM_PINMUX_PAD3;
	usart_conf.baudrate    = 57600;													//szybkoœæ transmisji UART
	usart_conf.stopbits   = USART_STOPBITS_1;

	stdio_serial_init(&cdc_uart_module, EDBG_CDC_MODULE, &usart_conf);
	usart_register_callback(&cdc_uart_module, (usart_callback_t)usart_receive, USART_CALLBACK_BUFFER_RECEIVED);
	
// 	#if C_USB
// 	usart_register_callback(&cdc_uart_module, (usart_callback_t)usart_transmitted, USART_CALLBACK_BUFFER_TRANSMITTED);
// 	#endif //C_USB
	
	usart_enable(&cdc_uart_module);
	usart_enable_callback(&cdc_uart_module, USART_CALLBACK_BUFFER_RECEIVED);
	
// 	#if C_USB
// 	usart_enable_callback(&cdc_uart_module, USART_CALLBACK_BUFFER_TRANSMITTED);
// 	#endif
}


// Dzielenie wejœciowego stringu z wierza poleceñ na pojedyncze argumenty
// - CmdLine	- wiersz poleceñ wpisany z klawiatury
// - argc		- wskaŸnik, przez który zwracana jest liczba znalezionych argumentów
// - argv		- wskaŸnik do tablicy wskaŸników, w której zapisywane s¹ wskaŸniki do kolejnych argumentów
void command_split(uint8_t *buf, uint8_t *argc, uint8_t *argv[])
{
	uint8_t		*char_pointer = buf;
	uint8_t		arg_count = 0;
	bool		new_arg = true;
	bool		string_mode = false;
	
	//sprawdza kolejne chary a¿ do pustego znaku
	while (*char_pointer != 0)
	{
		switch (*char_pointer)
		{
			case '"':									//je¿eli cudzys³ów
				wifi_name_flag ^= true;					//to pocz¹tek (lub koniec) nazwy wifi zawieraj¹cej spacje, mo¿e byæ u¿ywane przy ka¿dym poleceniu
				*char_pointer = 0;
			break;
			
			case ' ':									//spacja - nowy argument
				if (wifi_name_flag == false)
				{
					if (string_mode == false)
					{
						new_arg = true;
						*char_pointer = 0;				//zamiana spacji na null
					}
					else
					{
						if(new_arg)
						{
							argv[arg_count++] = char_pointer;
							new_arg = false;
						}
					}
				}
				else
				{
					if (new_arg)
					{
						argv[arg_count++] = char_pointer;
						new_arg = false;
					}
				}
			break;
			
			//inny znak ni¿ spacja
			default:
				//pocz¹tek nowego argumentu
				if (new_arg)
				{
					argv[arg_count++] = char_pointer;
					new_arg = false;
				}
			break;
		}
		//przesuniêcie wskaŸnika na nastêpny przes³any znak
		char_pointer++;
	}
	*argc = arg_count;
	wifi_name_flag = false;
}



// Porównuje badany string do wzorcowego a¿ do:
// - napotkania NULL w obu stringach
// - pierwszego ró¿nego znaku w obu stringach
// Tak skonstrukowana funkcja zajmuje mniej miejsca ni¿ strcmp() z biblioteki string.h
static inline bool compare_string(const char *string1, const char* string2)
{
	while(1)
	{
		if((*string1 == 0) && (*string2 == 0)) return true;		// Doszliœmy do koñca obu stringów, czyli s¹ sobie równe
		if(*string1++ != *string2++) return false;				// Przy pierwszej napotkanej ró¿nicy zwróæ fa³sz
	}
}


//funkcja sprawdzaj¹ca ci¹g znaków z konsoli z tablic¹ komend
static inline void (*find_cmd_pointer(uint8_t * EnteredName))(uint8_t argc, uint8_t * argv[])
{
	for(uint8_t i=0; i<(sizeof(commands_list)/sizeof(commands_list[0])); i++) {
		if(compare_string((const char *)EnteredName, commands_list[i].command_name)) {
			return commands_list[i].func;
		}
	}
	return NULL;
}

//podzielenie paczki UART na polecenie i argumenty, nastêpnie porównanie z tablic¹ poleceñ
void command_compare(void)
{
	uint8_t argc = 0;
	uint8_t *argv[MAX_ARGS];											//tablica przechowuj¹ca argumenty lub numery kart
	memset(argv, 0, sizeof(argv));										//czyszczenie tablicy
	buffer_usart_last = buffer_usart;
	uart_buffer_clear();
	command_split(buffer_usart_last.buffer, &argc, argv);				//dzielenie ci¹gu znaków z bufora uart na argumenty

	//je¿eli funkcja rozpoznana w tablicy komend to sprawdza argumenty
	command_pointer = find_cmd_pointer(argv[0]);
	if (command_pointer)
	{
		delay_ms(1);
		uart_send_char(ACK);
		//printf("%c", ACK);												//potwierdzenie odebrania polecenia
		command_pointer(argc, argv);									//wykonanie odpowiedniej funkcji
	}
	else
	{
		uart_send_char(NAK);
		//printf("%c", NAK);												//b³êdne polecenie
		crc_interpreter = 0;
	}
}


//paczki kart na listê UID mo¿liwych do przes³ania przez UART
void parse_cards_list(uint8_t *buffer_pointer)
{
	volatile uint8_t * pointer = buffer_pointer;
	uint8_t argc = 0;
	uint8_t *argv[MAX_ARGS];
	memset(argv, 0, sizeof(argv));
	struct serials_struct serials_list;
	uint16_t i = 0;										//zerowanie wskaŸnika bufora znaków paczki akrt
	memset(serials_list.buffer, 0, MAX_SERIALS_CHAR_COUNT);
	
	
	//wrzucenie znaków z kopii bezpoœrednio do serials buffer jeœli na koñcu poprzedniej paczki by³o niepe³ne UID
	while (last_card_number[i] != 0)
	{
		serials_list.buffer[i] = last_card_number[i];
		i++;
	}
	
	//while jeœli odczyt pierwszej paczki czyli jeœli list_start_flag = false
	if (!list_start_flag)
	{
		//dodaæ wykrywanie NULLa - jeœli nie znajdzie [ to b³êdna paczka odebrana z serwera
		
		while (*pointer++ != '[');					//czeka na pocz¹tek listy - rozpoznany po nawiasach kwadratowych
		list_start_flag = true;
		//dla paczki pierwszej
		send_command_check("list-ready");			//niekoniecznie dobre miejsce - mo¿e nast¹piæ b³¹d w przetwarzaniu paczki
	}
	
	
	//jeœli jest w œrodku listy odczytuje litery lub cyfry oraz przecinek, wrzuca do bufora uarta
	
	
	//warunek dla pe³nej paczki albo dla paczki koñcowej - dla pe³nej paczki czeka na null, dla paczki koñcowej czeka na koniec listy
	while (*pointer != ']' && *pointer != 0)										//do zmiany - mo¿e zapêtliæ
	{
		if (*pointer > '/' && *pointer < 'G') serials_list.buffer[i++] = *pointer++;			//znak do dodania
		else if (*pointer++ == ',') serials_list.buffer[i++] = ' ';
	}
	
	if (*pointer == ']') list_start_flag = false;
	
	command_split(serials_list.buffer, &argc, argv);				//funkcja do podzia³u argumentów, wykorzyst. te¿ do podzia³u listy kart
	
	
	//kopiowanie - NIE DLA OSTATNIEJ PACZKI, dla ostatniej paczki od razu wysy³anie
	memset(last_card_number, 0, 20);
	if (argv[argc-1] != 0 && serials_list.buffer[i-1] != 0 && list_start_flag)				//znak o inteksie i-1 jest nullem gdy ostatni odebrany znak by³ przecinkiem
	{
		sprintf((char *)last_card_number, "%s", (char *)argv[argc-1]);
		argv[argc-1] = 0;
		//kopiuj nr karty jeœli nie by³o na koñcu spacji
	}	
	
	//jeœli list_start_flag to paczka nie jest ostatnia - wtedy pomiñ przy wysy³aniu ostatni argument
	//bêdzie on wys³any jako pierwszy w nastêpnej paczce
	
	list_start_flag = true;
	//dla ka¿dej paczki
	for (uint8_t j=0; j<argc; j++)
	{
		if (argv[j] != 0)
		{
			while (!list_get_flag);
			list_get_flag = false;
			uart_buffer_clear();
			uart_add_to_buffer((char *)argv[j]);
			send_data();
		}
	}
	
	if (*pointer == ']') list_start_flag = false;

	
}

//funkcja obs³uguj¹ca odebrane znaki z UARTa
enum Console_Res_t usart_receive(void)
{
	if (cert_write_flag)
	{
		if ((received_char != '\n') && (received_char != '\r') && (received_char != ' '))
		{
			buffer_usart.buffer[buffer_usart.received_count++] = received_char;
		}
	}
	
	//porównanie sumy kontrolnej odebranej z obliczon¹
	else if (checksum_flag)
	{
		if (US == buffer_usart.buffer[buffer_usart.received_count])
		{
			if (received_char != crc_H_interpreter) checksum_error = true;		//jeœli jeden znak wczeœniej by³o US to sprawdzamy starszy bajt sumy CRC, jeœli siê ró¿ni¹ to b³¹d
			
			buffer_usart.received_count++;
		}
		else if (US == buffer_usart.buffer[buffer_usart.received_count-1])
		{
			if (received_char != crc_L_interpreter) checksum_error = true;		//jeœli US 2 znaki wczeœniej to sprawdzamy m³odszy bajt CRC, jeœli siê ró¿ni¹ to b³¹d
			
			buffer_usart.received_count++;
			
		}
		else if (US == buffer_usart.buffer[buffer_usart.received_count - 2])
		{
			//jeœli polecenie - sprawdŸ z tokenem polecenia
			
			//jeœli odpowiedŸ - sprawdŸ z tokenem odpowiedzi
			
			buffer_usart.received_count -= 2;
			buffer_usart.buffer[buffer_usart.received_count] = 0;			//wywalenie US ¿eby sprawdziæ poprzedni bufor
			
			//jeœli polecenie - sprawdŸ bufor i póŸniej token, wykonaj lub BEL
			//jeœli odpowiedŸ - sprawdŸ token TYLKO
			
			if (checksum_error)
			{
				checksum_error = false;
				uart_send_buffer((uint8_t *)NAK);
				//printf("%c", NAK);
			}
			
			else if (received_command_flag)
			{
				uart_flag = false;
				if (buffer_usart.buffer == buffer_usart_last.buffer)			//sprawdzenie bufora poprzedniego polecenia
				{
					if (received_char == token_read)
					{
						//printf("%c", BEL);			//jeœli bufor i token takie same
						uart_send_buffer((uint8_t *)ACK);
						//printf("%c", ACK);									//DC3 - ponowne wys³anie DANYCH
						usart_write_buffer_wait(&cdc_uart_module, buffer_response.buffer, buffer_response.received_count);
					}
					else
					{
						token_read = received_char;
						command_compare();										//jeœli to samo polecenie z innym tokenem
					}
				}
				else 
				{
					token_read = received_char;
					command_compare();											//jeœli inne polecenie
				}
				received_command_flag = false;
			}
			else
			{
				//jeœli przysz³a odpowiedŸ
				if (received_char != token_send)
				{
					uart_send_buffer((uint8_t *)DC3);
					//printf("%c", DC3);				//jeœli token nie zgadza siê z wys³anym
				}
				else token_send++;
			}
			//koniec obs³ugi ostatniego znaku
			
			checksum_flag = false;
		}
		return Console_OK;
	}
	else
	{
		//sprawdzenie czy znaki s¹ "readable"
		if (received_char >= ' ' && received_char <= '~' && !send_command_flag)
		{
			if (buffer_usart.received_count >= MAX_RX_BUFFER_LENGTH)				//je¿eli bufor pe³en zwraca kod b³êdu
			{
				return Console_BufferFull;
			}
			else
			{	
				if (!uart_flag) uart_flag = true;
				buffer_usart.buffer[buffer_usart.received_count++]=received_char;	//wrzucenie znaku do bufora
				return Console_OK;
			}
		}
	
		
		//obs³uga pozosta³ych znaków kontrolnych
		if (!send_command_flag)
		{
			switch (received_char)
			{
			case DC3:
				uart_send_buffer((uint8_t *)ACK);
				//printf("%c", ACK);									//DC3 - ponowne wys³anie DANYCH
				usart_write_buffer_wait(&cdc_uart_module, buffer_response.buffer, buffer_response.received_count);
				return Console_ReceiveError;
				break;
// 			case ACK:
// 				uart_buffer_clear();
// 				command_flag = false;
// 				return Console_ReceivedACK;
// 				break;
			case DC1:
				if(buffer_usart.buffer[0] != 0)
				{
					memset(buffer_usart.buffer, 0, MAX_RX_BUFFER_LENGTH);
					buffer_usart.received_count = 0;
				}
				checksum_error = false;
				checksum_flag = false;
				crc_interpreter = 0;
				
				//flaga pocz¹tku komendy
				received_command_flag = true;
				
				return Console_ReceivedCommand;
				break;
				
			case DC2:
				if(buffer_usart.buffer[0] != 0)
				{
					memset(buffer_usart.buffer, 0, MAX_RX_BUFFER_LENGTH);
					buffer_usart.received_count = 0;
				}
				checksum_error = false;
				checksum_flag = false;
				crc_interpreter = 0;
				
				received_command_flag = true;
				//sprawdziæ, czy odpowiedŸ zosta³a ju¿ wys³ana!!!
				//jeœli wys³ana to co?
				//jeœli nie wys³ana to co?
				
				return Console_ReceivedRetry;
				break;
			
			case ENQ:
				uart_send_buffer((uint8_t *)DC4);
				//printf("%c", DC4);
				break;
			
			case US:
				checksum_flag = true;			//ustawienie flagi - kolejny znak porównany z obliczonym xorem
				//obliczenie xora z otrzymanego polecenia, zapisanie do zmiennej
				crc_interpreter = 0;
				crc_interpreter = crc_calculate(buffer_usart.buffer, buffer_usart.received_count);
				crc_H_interpreter = (uint8_t)(crc_interpreter >> 8);
				crc_L_interpreter = (uint8_t)(crc_interpreter);
				buffer_usart.buffer[buffer_usart.received_count]=received_char;
				return Console_ReceivedCRC;
				break;
				
			case CR:
				if (buffer_usart.received_count && !checksum_error && return_flag)
				{
					//printf("\r\n");
					command_compare();
					//printf("\r\n");
					buffer_usart.received_count=0;
					uart_flag = false;
					return_flag = false;
					return Console_ReceivedCommand;
				}
				break;
			case SOH:
				//czyszczenie bufora przed odebraniem polecenia
				if(buffer_usart.buffer[0] != 0)
				{
					memset(buffer_usart.buffer, 0, MAX_RX_BUFFER_LENGTH);
					buffer_usart.received_count = 0;
				}
				checksum_error = false;
				return_flag = true;
				crc_interpreter = 0;
				return Console_OK;
				break;
			default:
				return Console_OK;
				break;
				
				//przyjdzie polecenie - zrobiæ to co w przypadku SOH
			
// 				if (buffer_usart.received_count && !checksum_error && return_flag)
// 				{
// 					command_compare();
// 					buffer_usart.received_count=0;
// 					uart_flag = false;
// 					return_flag = false;
// 					return Console_ReceivedCommand;
// 				}
// 				else
// 				{
// 					if (checksum_error)
// 					{
// 						printf("%c", NAK);
// 						printf("%c%c", (uint8_t)(crc_interpreter << 8), (uint8_t)crc_interpreter);
// 						crc_interpreter = 0;
// 						return_flag = false;
// 					}
// 					return Console_OK;
// 				}
// 				break;
// 			
// 			case SYN:
// 				printf("%c", DC4);
// 				return Console_OK;
// 				break;
// 			case ESC:
// 				uart_buffer_clear();
// 				printf("\r\n > %c",  DC4);
// 				return Console_OK;
// 				break;
// 			case BACKSPACE1:
// 			case BACKSPACE2:
// 				buffer_usart.received_count--;
// 				buffer_usart.buffer[buffer_usart.received_count] = 0;
// 				return Console_OK;
// 				break;
// 			case ETX:
// 				return Console_OK;
// 				break;
// 			case 0x13:							//bez tego case'a wchodzi³ zawsze w ACK
// 				return Console_OK;
// 				break;
// 			default:
// 				return Console_OK;
// 				break;
			}
		}
		
		//obs³uga znaków kontrolnych po wys³aniu komendy
		else if (send_command_flag)
		{
			switch (received_char)
			{
			case NAK:												//NAK - ponowne wys³anie POLECENIA
				usart_write_buffer_wait(&cdc_uart_module, buffer_usart.buffer, buffer_usart.received_count);
				return Console_ReceiveError;
				break;
			case ACK:
				uart_buffer_clear();
				send_command_flag = false;
				return Console_ReceivedACK;
				break;
			default:
				return Console_OK;
				break;
			}
		}
		
	}
	return Console_OK;
}

static enum CmdRes_t Parse_DecChar(const uint8_t * InputChar, uint8_t * OutputChar)
{
	if(*InputChar >= '0' && *InputChar <= '9') {
		*OutputChar = *InputChar - '0';
		return Cmd_OK;
	}
	else {
		return Cmd_ExpectedDec;
	}
}

enum CmdRes_t parse_dec16(const uint8_t * Argument, uint16_t * Output, const uint16_t MaxValue)
{
//	const uint16_t MaxValue = 65535;
//	const uint8_t * OrgArgument = Argument;
	uint8_t Digit;
	uint16_t Temp = 0;
	uint16_t Temp2;
	enum CmdRes_t Result;

	// Kontrola czy podano argument
	if(Argument == NULL) {
		Result = Cmd_MissingArgument;
		goto End;
	}

	// Przetwarzamy wszystkie znaki po kolei
	while(*Argument != 0) {
		Result = Parse_DecChar(Argument++, &Digit);
		if(Result) {
			goto End;
		}

		Temp2 = Temp * 10 + Digit;
		if(Temp <= Temp2) {
			Temp = Temp2;
		}
		else {
			Result = Cmd_Overflow;
			goto End;
			//return Cmd_Overflow;
		}
	}

	// Zwracanie wyniku
	if(Temp <= MaxValue) {
		*Output = Temp;
	}
	else {
		Result = Cmd_Overflow;
	}
	
	// Wyœwietlenie informacji o ewentualnym b³êdzie i zwrócenie wyniku
	End:
// 	if(Result) {
// 		Console_Debug(Result, OrgArgument);
// 	}
	return Result;
}

//przerobienie znaku ascii na hex
enum CmdRes_t parse_char_to_hex(const uint8_t * InputChar, uint8_t * OutputChar)
{
	if(*InputChar >= '0' && *InputChar <= '9') {
		*OutputChar = *InputChar - '0';
		return Cmd_OK;
	}
	else if(*InputChar >= 'A' && *InputChar <= 'F') {
		*OutputChar = *InputChar - 55;
		return Cmd_OK;
	}
	else if(*InputChar >= 'a' && *InputChar <= 'f') {
		*OutputChar = *InputChar - 87;
		return Cmd_OK;
	}
	else {
		return Cmd_ExpectedHex;
	}
}

//przerobienie ci¹gu wejœciowego na hex8
void parse_HEX8(const uint8_t * input, uint8_t * output)
{
	uint8_t ByteH;
	uint8_t ByteL;

	// Przetwarzanie bajtu 0
	parse_char_to_hex(input++, &ByteH);

	// Przetwarzanie bajtu 1
	parse_char_to_hex(input++, &ByteL);

	// Zwracanie wyniku
	*output = ByteH << 4 | ByteL;
}

//przerobienie ci¹gu wejœciowego na hex8 int (na potrzeby przetworzenia temperatury w logu)
void parse_HEX8_int(const uint8_t * input, int8_t * output)
{
	uint8_t ByteH;
	uint8_t ByteL;

	// Przetwarzanie bajtu 0
	parse_char_to_hex(input++, &ByteH);

	// Przetwarzanie bajtu 1
	parse_char_to_hex(input++, &ByteL);

	// Zwracanie wyniku
	*output = (int8_t)(ByteH << 4 | ByteL);
}

//przerobienie ci¹gu wejœciowego na hex16
void parse_HEX16(const uint8_t * input, uint16_t * output)
{
	uint8_t Nibble;
	uint32_t Temp = 0;
	
	for(uint8_t i=12; i!=252; i=i-4)
	{
		parse_char_to_hex(input++, &Nibble);
		Temp |= ((uint32_t)Nibble << i);
	}
	*output = Temp;
}

void parse_HEX24(const uint8_t * input, uint32_t * output)
{
	uint8_t Nibble;
	uint32_t Temp = 0;
	
	for(uint8_t i=20; i!=252; i=i-4)
	{
		parse_char_to_hex(input++, &Nibble);
		Temp |= ((uint32_t)Nibble << i);
	}
	*output = Temp;
}

void parse_HEX32(const uint8_t * input, uint32_t * output)
{
	uint8_t Nibble;
	uint32_t Temp = 0;
	
	for(uint8_t i=28; i!=252; i=i-4)
	{
		parse_char_to_hex(input++, &Nibble);
		Temp |= ((uint32_t)Nibble << i);
	}
	*output = Temp;
}

enum CmdRes_t Parse_HexString(const uint8_t * InputString, uint8_t * OutputString, uint8_t * OutputLength, const uint8_t MaxLength, const uint8_t MinLength) {
	
	// WskaŸniki aktualnie przetwarzanych znaków
	const uint8_t * OrgArgument = InputString;
	uint8_t NibbleH;
	uint8_t NibbleL;
	*OutputLength = 0;
	enum CmdRes_t Result;

	// przetwarzanie a¿ do napotkania znaku 0
	while(*InputString != 0) {
		
		// Kontrola przepe³nienia
		if(*OutputLength == MaxLength) {
			Result = Cmd_Overflow;
			goto End;
		}
		
		// Pomijanie spacji
		if(*InputString == ' ') {
			InputString++;
			continue;
		}

		// Przetwarzanie starszego nibble
		Result = parse_char_to_hex(InputString++, &NibbleH);
		if(Result) {
			goto End;
			//return Result;
		}

		// Przetwarzanie m³odszego nibble
		Result = parse_char_to_hex(InputString++, &NibbleL);
		if(Result) {
			goto End;
		}

		// Sklejanie wyniku
		*OutputString++ = NibbleH << 4 | NibbleL;

		// Licznie znaków w stringu wynikowym
		(*OutputLength)++;
	}

	// Kontrola d³ugoœci
	if(*OutputLength < MinLength) {
		Result = Cmd_Underflow;
	}
	
	// Wyœwietlenie informacji o ewentualnym b³êdzie i zwrócenie wyniku
	End:
// 	if(Result) {
// 		Console_Debug(Result, OrgArgument);
// 	}
	return Result;
}

// Look-up table do konwersji base64
static const uint8_t Base64LookUpTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static uint8_t Base64find(uint8_t Char) {
	
	// Przeszukiwanie tablicy
	for(uint8_t i=0; i<sizeof(Base64LookUpTable); i++) {
		if(Base64LookUpTable[i] == Char) {
			return i;
		}
	}
	
	// Znak =
	return 0;
}
// Konwersja tablicy z danymi w formacie Base64 na format binarny
void Base64decode(uint8_t OutputBin[], uint8_t * InputBase64, uint16_t InputLength, uint16_t * OutputLength) {
	
	uint8_t Buffer[4];

	// Przetwarzanie wszystkich znaków z InputBase64
	while(InputLength) {
		
		// W ka¿dym obiegu pêtli przetwarzamy 4 znaki
		InputLength = InputLength - 4;
		
		// Dekodowanie symboli Base64 na wartoœci binarne
		for (uint8_t i=0; i<4; i++) {
			Buffer[i] = Base64find(*InputBase64++);
		}

		// Zwracanie wyniku
		OutputBin[(*OutputLength)++] = (Buffer[0] << 2) + (Buffer[1] >> 4);
		OutputBin[(*OutputLength)++] = (Buffer[1] << 4) + (Buffer[2] >> 2);
		OutputBin[(*OutputLength)++] = (Buffer[2] << 6) + (Buffer[3] >> 0);
	}
}

void Base64encode(uint8_t * OutputBase64, const uint8_t * InputBin, uint8_t InputLength)
{
	uint8_t Buffer[4] = {0,0,0,0};				// Bufor wyjœciowy - cztery znaki w kodowaniu Base64
	uint8_t Byte = 0;							// Numer bajtu w aktualnie przetwarzanej trójce

	// Przetwarzanie wszystkich znaków z InputBin
	while(InputLength--) {

		// W ka¿dym obiegu pêtli przetwarzanie po jednym bajcie z InputBin do bufora trójki 
		Byte++;
		
		// Przetwarzanie pierwszego bajtu
		if(Byte == 1) {

			// Przetwarzamy pierwszy bajt na Base64
 			Buffer[0]  = (((*InputBin) & 0b11111100) >> 2);
 			Buffer[1]  = (((*InputBin) & 0b00000011) << 4);

			// Wysy³amy pierwszy znak Base64 do bufora wyjœciowego
			*OutputBase64++ = Base64LookUpTable[Buffer[0]];
 			
			// Je¿eli to by³ ostatni bajt w buforze wejœciowym
			if(InputLength == 0) {
				*OutputBase64++ = Base64LookUpTable[Buffer[1]];
				*OutputBase64++ = '=';
				*OutputBase64++ = '=';
			}
		}			

		// Przetwarzanie drugiego bajtu
		else if(Byte == 2) {

			// Przetwarzamy drugi bajt na Base64
 			Buffer[1] |= (((*InputBin) & 0b11110000) >> 4);
 			Buffer[2]  = (((*InputBin) & 0b00001111) << 2);

			// Wysy³amy drugi znak Base64 do bufora wyjœciowego
			*OutputBase64++ = Base64LookUpTable[Buffer[1]];
 			
			// Je¿eli to by³ ostatni bajt w buforze wejœciowym
			if(InputLength == 0) {
				*OutputBase64++ = Base64LookUpTable[Buffer[2]];
				*OutputBase64++ = '=';
			}
		}
		
		// Przetwarzanie trzeciego bajtu
		else if (Byte == 3) {		
			
			// Przetwarzamy trzeci bajt na Base64							
 			Buffer[2] |= (((*InputBin) & 0b11000000) >> 6);
 			Buffer[3]  = (((*InputBin) & 0b00111111) << 0);

			// Wysy³amy trzeci i czwarty znak Base64 do bufora wyjœciowego
			*OutputBase64++ = Base64LookUpTable[Buffer[2]];
			*OutputBase64++ = Base64LookUpTable[Buffer[3]];

			// Zerowanie licznika bajtów trójki
			Byte = 0; 
		}

		// Inkrementacja wskaŸnika bufora wejœciowego
		InputBin++;
	}
}