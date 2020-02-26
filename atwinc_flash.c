#include "atwinc_flash.h"
#include "interpreter.h"
#include "timer_samw.h"
#include "ASF/common/components/wifi/winc1500/driver/include/m2m_wifi.h"
#include "ASF/common/components/wifi/winc1500/programmer/programmer.h"
#include "delay.h"
#include "tls_setup.h"
#include "tls_srv_sec.h"

extern struct usart_module cdc_uart_module;
extern struct buffer_struct buffer_usart;
extern uint16_t received_char;
tstrFileInfo root_tls_certs[2];

//funkcja odczytu certyfikatu root CA oraz certyfikatu TLS z kluczem (u¿ywane przy MQTT)
void cert_read(void)
{
	uint8 buffer[2 * FLASH_SECTOR_SZ];						//bufor o rozmiarze 2 sektorów - tyle pamiêci zarezerwowane na certyfikaty i klucze TLS
	memset(&buffer, 0, 2 * FLASH_SECTOR_SZ);
	m2m_wifi_download_mode();								//uruchomienie atwinca w trybie download daj¹cym dostêp do pamiêci
	
	spi_flash_read(buffer, M2M_TLS_ROOTCER_FLASH_OFFSET, M2M_TLS_ROOTCER_FLASH_SIZE);		//odczyt pamiêci root CA
	usart_write_buffer_wait(&cdc_uart_module, buffer, M2M_TLS_ROOTCER_FLASH_SIZE);			//przes³anie przez UART
	
	printf("\r\n\r\n=====================================================================================\r\n\r\n");
	
	memset(&buffer, 0, 2 * FLASH_SECTOR_SZ);
	spi_flash_read((uint8 *)buffer, M2M_TLS_SERVER_FLASH_OFFSET, M2M_TLS_SERVER_FLASH_SIZE);	//odczyt pamiêci certyfikatów i kluczy TLS
	usart_write_buffer_wait(&cdc_uart_module, buffer, M2M_TLS_SERVER_FLASH_SIZE);				//przes³anie przez UART
	m2m_wifi_deinit(NULL);																		//wy³¹czenie atwinca
}


//funkcja zapisu certyfikatu TLS i klucza do pamiêci atwinca
void cert_write(void)
{
	uint8_t atwinc_cert_table[5 * 1024];				//za ma³o ramu na tablicê dla 2 sektorów (8KB)
	uint8_t divider_count = 0;							//licznik myœlników - do pominiêcia nag³ówków i stópek begin/end certificate
	uint16_t atwinc_cert_table_count = 0;				//licznik znaków w tablicy (mog¹ pojawiæ siê nulle wiêc strlen odpada)
	
	memset(&atwinc_cert_table, 0, sizeof(atwinc_cert_table));
	
	printf("Waiting for KEY file\r\n");
	
	//pêtla przerzucaj¹ca znaki z bufora uarta do bufora odbiorczego certyfikatu/klucza
	while (1)
	{
		if (buffer_usart.received_count >= 254)			//przed przepe³nieniem bufora uarta skopiowaæ zawartoœæ do bufora base64
		{
			memcpy(&atwinc_cert_table[atwinc_cert_table_count], &buffer_usart.buffer, buffer_usart.received_count);
			atwinc_cert_table_count += buffer_usart.received_count;
			uart_buffer_clear();
		}
		
		//liczenie myœlników
		if (received_char == '-')
		{
			divider_count++;
			received_char = 0;
		}
		
		if (divider_count == 10)			//doliczone do 10 - koniec nag³ówka, czyszczenie bufora uarta, rozpoczêcie odbierania treœci certyfikatu
		{
			uart_buffer_clear();
			divider_count++;
		}
		
		if ((divider_count == 12) && (buffer_usart.received_count >= 2))		//wartoœæ 12 - pierwszy myœlnik stopki, odebrano ca³y certyfikat
		{																	//jeœli czêœæ nie zosta³a skopiowana do bufora base64 - skopiuj
			memcpy(&atwinc_cert_table[atwinc_cert_table_count], &buffer_usart.buffer, --buffer_usart.received_count);
			atwinc_cert_table_count += buffer_usart.received_count;
			uart_buffer_clear();
		}
		
		//je¿eli doliczone do 21 - odebrano ca³y plik ³¹cznie ze stopk¹
		if (divider_count == 21)
		{
			uart_buffer_clear();
			divider_count = 0;
			break;
		}
	}
	uint16_t key_count = 1200;
	uint8_t key_buffer[key_count];
	memset(&key_buffer, 0, sizeof(key_buffer));
	key_count = 0;
	
	//dekodowanie base64 i przerzucanie zdekodowanych znaków do bufora pamiêci atwinca
	Base64decode(key_buffer, atwinc_cert_table, atwinc_cert_table_count, &key_count);
	memset(&atwinc_cert_table, 0, sizeof(atwinc_cert_table));
	atwinc_cert_table_count = 0;

	
	printf("Waiting for CRT file\r\n");
	while (1)													//jak wy¿ej
	{
		if (buffer_usart.received_count >= 254)
		{
			memcpy(&atwinc_cert_table[atwinc_cert_table_count], &buffer_usart.buffer, buffer_usart.received_count);
			atwinc_cert_table_count += buffer_usart.received_count;
			uart_buffer_clear();
		}
		
		if (received_char == '-')
		{
			divider_count++;
			received_char = 0;
		}
		
		if (divider_count == 10)
		{
			uart_buffer_clear();
			divider_count++;
		}
		
		if ((divider_count == 12) && (buffer_usart.received_count >= 2))
		{
			memcpy(&atwinc_cert_table[atwinc_cert_table_count], &buffer_usart.buffer, --buffer_usart.received_count);
			atwinc_cert_table_count += buffer_usart.received_count;
			uart_buffer_clear();
		}
		
		if (divider_count == 21)
		{
			uart_buffer_clear();
			divider_count = 0;
			break;
		}
	}
	
	uint16_t cert_count = 900;	
	uint8_t cert_buffer[cert_count];
	memset(&cert_buffer, 0, cert_count);
	cert_count = 0;
	
	Base64decode(cert_buffer, atwinc_cert_table, atwinc_cert_table_count, &cert_count);
	memset(&atwinc_cert_table, 0, sizeof(atwinc_cert_table));
	
	//przerobienie wygenerowanych plików, posklejanie, odczytanie numerów seryjnych
	
	
	//ReadServerX509Chain albo samo CryptoX509CertDecode			w ReadServerX509Chain u¿yte: struktura, certyfikatu (treœæ i d³ugoœæ), liczba ogniw ³añcucha, struktura finalna
	tstrX509Entry *pstrServerCert = NULL;							//struktura, do której kopiowane s¹ parametry certyfikatu ze struktury strX509
	tstrASN1RSAPrivateKey	strRSAPrivKey;
	tstrBuff astrCertBuffs[3];
	astrCertBuffs[0].pu8Data = cert_buffer;
	astrCertBuffs[0].u16BufferSize = cert_count;
	astrCertBuffs[1].pu8Data = key_buffer;
	astrCertBuffs[1].u16BufferSize = key_count;
	ReadServerX509Chain(astrCertBuffs, 2, &pstrServerCert);
	
	
	memset(&strRSAPrivKey, 0, sizeof(tstrASN1RSAPrivateKey));
	
	sint8 ret = BuildServerX509CertChain(X509_CERT_PUBKEY_RSA, pstrServerCert, &strRSAPrivKey);
	
	
	//pstrServerCert - head
	
	
//	sint8 ret = CryptoDecodeRsaPrivKey(key_buffer, key_count, &strRSAPrivKey);
	
	
	asm("BKPT");
	
	
	//dalej CryptoDecodeRsaPrivKey
	//albo BuildServerX509CertChain
	//ReadServerX509Chain tak¿e dla klucza
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
// 	txtrX509CertInfo strX509;										//struktura przechowuj¹ca zdekodowany certyfikat wraz z datami wa¿noœci, numerem seryjnym, common name itd
// 	CryptoX509CertDecode(cert_buffer, cert_count, &strX509, 0);
	
	//w miêdzyczasie kopiowane niektóre dane
	
	
	
	
	
	
	
	//póŸniej
	//CryptoDecodeRsaPrivKey
	//i BuildServerX509CertChain
	
	
	//tls_srv_sec.c
	//w funkcji: RsaBuildServerX509CertChain
	
	//ValidateKeyMaterial
	
	//zapisanie klucza
	//TlsSrvSecFopen		funkcja odpowiedzialna za tworzenie nag³ówka - numery seryjne, nazwy plików list - to co wczeœniej by³o sta³e
	//TlsSrvSecFwrite		
	//TlsSrvSecFclose		funkcja odpowiedzialna za tworzenie stopki - sta³y ci¹g znaków + nry seryjne
	
	//tworzenie listy certyfikatów (rsa.lst, ecdsa.lst) funkcjami:
	//TlsSrvSecFopen
	//TlsSrvSecFwrite
	//TlsSrvSecFclose
	
	
	
	
	
	
	
	
	uint32 pu32SecSz;
	
	root_tls_certs[0].pu8FileData = key_buffer;
	root_tls_certs[0].u32FileSz = (uint32_t)key_count;
	root_tls_certs[1].pu8FileData = cert_buffer;
	root_tls_certs[1].u32FileSz = (uint32_t)cert_count;
	
	m2m_wifi_download_mode();
	
	ret = TlsSrvSecWriteCertChain(root_tls_certs[0].pu8FileData, root_tls_certs[0].u32FileSz, &root_tls_certs[1], 2, atwinc_cert_table, &pu32SecSz, TLS_SRV_SEC_MODE_WRITE);
	if (ret != M2M_SUCCESS)
	{
		printf("\r\n\r\n\r\n\r\nERROR");
	}
	
// 	usart_write_buffer_wait(&cdc_uart_module, key_buffer, FLASH_SECTOR_SZ/2);
// 	printf("\r\n\r\n\r\n\r\n=======================================================================\r\n\r\n\r\n\r\n");
// 	usart_write_buffer_wait(&cdc_uart_module, cert_buffer, FLASH_SECTOR_SZ/2);
	
	
	
	
	//fragment oddzielaj¹cy certyfikat od klucza
// 	uint8_t atwinc_divider[] = {
// 		0xFF, 0xFF, 0x00, 0x01, 0x03, 0x00, 0x00, 0x01, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x34, 0x7D, 0xA2, 0x00, 0x34, 0x7E, 0xA2, 0x00, 0x38, 0x7E, 0xA2, 0x00, 0x38, 0x7F, 0xA2, 0x00, 0xB8, 0x7F, 0xA2, 0x00, 0x38, 0x80, 0xA2, 0x00, 0xB8, 0x80, 0xA2, 0x00, 0x38, 0x81, 0xA2, 0x00
// 	};
	
	//skopiowanie "oddzielacza"
// 	memcpy(&atwinc_cert_table[atwinc_cert_table_count], &atwinc_divider, sizeof(atwinc_divider));
// 	atwinc_cert_table_count += sizeof(atwinc_divider);
	
	//dekodowanie klucza i przerzucenie do bufora
// 	Base64decode(atwinc_cert_table, base64_buffer, base64_count, &atwinc_cert_table_count);
	
	
	//zakoñczenie z numerami seryjnymi certyfikatu i klucza
// 	uint8_t atwinc_finish[] = {
// 		0xEF, 0xBB, 0xBF, 0x50, 0x52, 0x49, 0x56, 0x5F, 0x30, 0x30, 0x64, 0x33, 0x32, 0x65, 0x39, 0x62, 0x66, 0x33, 0x65, 0x30, 0x30, 0x36, 0x38, 0x35, 0x35, 0x30, 0x31, 0x64, 0x30, 0x66, 0x66, 0x33, 0x35, 0x39, 0x39, 0x39, 0x61, 0x65, 0x37, 0x30, 0x38, 0x31, 0x62, 0x65, 0x61, 0x61, 0x66, 0x61, 0x30, 0x30, 0x00, 0x43, 0x45, 0x52, 0x54, 0x5F, 0x30, 0x30, 0x64, 0x33, 0x32, 0x65, 0x39, 0x62, 0x66, 0x33, 0x65, 0x30, 0x30, 0x36, 0x38, 0x35, 0x35, 0x30, 0x31, 0x64, 0x30, 0x66, 0x66, 0x33, 0x35, 0x39, 0x39, 0x39, 0x61, 0x65, 0x37, 0x30, 0x38, 0x31, 0x62, 0x65, 0x61, 0x61, 0x66, 0x61, 0x30, 0x30, 0x00
// 	};
// 	memcpy(&atwinc_cert_table[atwinc_cert_table_count], &atwinc_finish, sizeof(atwinc_finish));
//	atwinc_cert_table_count += sizeof(atwinc_finish);
	
	//wyœwietlenie zawartoœci bufora
// 	usart_write_buffer_wait(&cdc_uart_module, atwinc_cert_table, FLASH_SECTOR_SZ);
// 
// 
// 	m2m_wifi_download_mode();																		//uruchomienie ATWINCa w trybie download - umo¿liwia zapis i odczyt flasha
// 	spi_flash_erase(M2M_TLS_SERVER_FLASH_OFFSET, M2M_TLS_ROOTCER_FLASH_SIZE);						//czyszczenie pamiêci certyfikatów
// 	spi_flash_write(atwinc_cert_table, M2M_TLS_SERVER_FLASH_OFFSET, atwinc_cert_table_count);		//zapis
// 	m2m_wifi_deinit(NULL);																			//wy³¹czenie ATWINCa
// 	atwinc_cert_table_count = 0;
// 	memset(&atwinc_cert_table, 0, 1024 * 5);														//czyszczenie bufora
}

//odczyt sektora pamiêci zawieraj¹cego stronê konfiguracyjn¹
void provision_page_read(void)
{
	m2m_wifi_download_mode();																		//uruchomienie w trybie download - umo¿liwia odczyt flasha
	uint8 buffer[M2M_HTTP_MEM_FLASH_SZ];
	memset(&buffer, 0, M2M_HTTP_MEM_FLASH_SZ);
	spi_flash_read((uint8 *)buffer, M2M_HTTP_MEM_FLASH_OFFSET, M2M_HTTP_MEM_FLASH_SZ);				//odczyt
	usart_write_buffer_wait(&cdc_uart_module, buffer, M2M_HTTP_MEM_FLASH_SZ);						//przes³anie pamiêci przez uart
	m2m_wifi_deinit(NULL);																			//wy³¹czenie
}

//zapisanie domyœlnej strony konfiguracyjnej we flashu - pozwala unikn¹æ aktualizacji oprogramowania ka¿dego ATWINCa
void provision_page_default(void)
{
	m2m_wifi_download_mode();			//tryb download - dostêp do pamiêci flash ATWINCa
	
	//utworzenie nowego bufora przechowuj¹cego pamiêæ strony konfiguracyjnej
	uint8_t provision_buf[M2M_HTTP_MEM_FLASH_SZ] = {};				//wklejenie zrzutu pamiêci w HEX
	
	//zapis do flasha atwinca
	spi_flash_erase(M2M_HTTP_MEM_FLASH_OFFSET, M2M_HTTP_MEM_FLASH_SZ);
	spi_flash_write((uint8 *)provision_buf, M2M_HTTP_MEM_FLASH_OFFSET, M2M_HTTP_MEM_FLASH_SZ);
	
	m2m_wifi_deinit(NULL);
	delay_ms(10);
}

//zapis domyœlnych certyfikatów + ROOT CA do AWSa
void root_cert_default(void)
{
	m2m_wifi_download_mode();
	
	//utworzenie nowego bufora przechowuj¹cego pamiêæ root certów
	uint8_t root_cert_buf[M2M_TLS_ROOTCER_FLASH_SIZE] = {};					//wklejenie zrzutu pamiêci w hex
	
	
	spi_flash_erase(M2M_TLS_ROOTCER_FLASH_OFFSET, 2*FLASH_SECTOR_SZ);									//czyszczenie pamiêci
	spi_flash_write((uint8 *)root_cert_buf, M2M_TLS_ROOTCER_FLASH_OFFSET, 2*FLASH_SECTOR_SZ);			//zapis domyœlnych certyfikatów do pamiêci ATWINCa

	m2m_wifi_deinit(NULL);
}

void atwinc_write_bridge_callback(uint8_t *buffer, uint16_t count)
{
	//przerzucenie received_char do buffer
	buffer[count] = received_char;
}