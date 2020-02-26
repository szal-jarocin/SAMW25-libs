#include "aws_mqtt.h"
#include "aws_iot_config.h"
#include "asf.h"
#include "connect_wifi.h"
#include "eeprom_emul.h"
#include "commands.h"
#include "ASCII_definitions.h"
#include "AWS_SDK/aws_iot_src/utils/aws_iot_version.h"
#include "AWS_SDK/aws_iot_src/protocol/mqtt/aws_iot_mqtt_interface.h"
#include "AWS_SDK/aws_iot_src/shadow/aws_iot_shadow_interface.h"
#include "driver/include/m2m_wifi.h"
#include "AWS_SDK/aws_mqtt_embedded_client_lib/MQTTClient-C/src/MQTTClient.h"
#include "interpreter.h"
#include "atecc608.h"

#include "driver/include/m2m_ssl.h"




extern uint8_t wifi_ssid[SSID_LENGTH+1];
extern uint8_t wifi_pass[PASSWORD_LENGTH+1];
extern uint8_t serial_num[SERIAL_LENGTH+1];
extern bool mqtt_connected_flag;
extern uint8 gbSocketInit;
extern Client c;

extern void (*command_pointer)(uint8_t argc, uint8_t *argv[]);

bool infinitePublishFlag = true;
MQTTConnectParams connectParams;
MQTTSubscribeParams subParams;
MQTTMessageParams Msg;
MQTTPublishParams Params;


static sint8 ecdsa_process_sign_verify_request(uint32 number_of_signatures)
{
    sint8 status = M2M_ERR_FAIL;
    tstrECPoint	Key;
    uint32 index = 0;
    uint8 signature[80];
    uint8 hash[80] = {0};
    uint16 curve_type = 0;
    
    for(index = 0; index < number_of_signatures; index++)
    {
        status = m2m_ssl_retrieve_cert(&curve_type, hash, signature, &Key);

        if (status != M2M_SUCCESS)
        {
            M2M_ERR("m2m_ssl_retrieve_cert() failed with ret=%d", status);
            return status;
        }

        if(curve_type == EC_SECP256R1)
        {
            bool is_verified = false;
            
            //status = atcab_verify_extern(hash, signature, Key.X, &is_verified);
            
			//weryfikacja podpisu wiadomoœci otrzymanej z serwera
			//wiadomoœæ - hash wyci¹gniêty funkcj¹ m2m_ssl_retreive_cert
			//podpis - signature j.w.
			//klucz - j.w., jedynie parametr X
			//zmienna bool, której stan zmienia siê po zweryfikowaniu podpisu
			
			
			//status = atecc608_Verify();
			//umieszczenie odebranego z serwera hasha w Message Digest Buffer
			status = ATECC608_MesDigBufSet(hash, 32);


			uint8_t response = 1;
			status = ATECC608_Verify(&response, ATECC608_VERIFY_MODE_EXTERNAL | ATECC608_VERIFY_SOURCE_MESDIGBUF | ATECC608_VERIFY_MAC_NONE, ATECC608_KEYTYPE_P256_NIST_ECC, signature, Key.X);
			if (response == 0) is_verified = true;
			else is_verified = false;
			
			if (response == 0) is_verified = true;
			else is_verified = false;			
			
			
			if(status == M2M_SUCCESS)
            {
                status = (is_verified == true) ? M2M_SUCCESS : M2M_ERR_FAIL;
                if(is_verified == false)
                {
                    M2M_INFO("ECDSA SigVerif FAILED\n");
                }
            }
            else
            {
                status = M2M_ERR_FAIL;
            }
            
            if(status != M2M_SUCCESS)
            {
                m2m_ssl_stop_processing_certs();
                break;
            }
        }
    }

    return status;
}


static sint8 ecdh_derive_client_shared_secret(tstrECPoint *server_public_key, uint8 *ecdh_shared_secret, tstrECPoint *client_public_key)
{
	sint8 status = M2M_ERR_FAIL;
	uint16_t key_id = 2;
	
	//wygeneruj nowy klucz publiczny
    if(ATECC608_GenKeyPublic(client_public_key->X, key_id) == M2M_SUCCESS)
    {
		status = ATECC608_TempKeySet(client_public_key->X, 64);
        client_public_key->u16Size = 64;
        //do the ecdh from the private key in tempkey, results put in ecdh_shared_secret
		//ECDH dla klucza trzymanego w tempkey, wynikowy shared secret do ecdh_shared_secret
		if(ATECC608_ECDH(ecdh_shared_secret, ATECC608_ECDH_SOURCE_TEMPKEY | ATECC608_ECDH_DEST_OUTPUT, key_id, server_public_key->X) == M2M_SUCCESS)
        {
            status = M2M_SUCCESS;
        }
    }

    return status;
}


static void ecc_process_request(tstrEccReqInfo *ecc_request)
{
	tstrEccReqInfo ecc_response;
	uint8 signature[80];
	uint16 response_data_size = 0;
	uint8 *response_data_buffer = NULL;
	
	ecc_response.u16Status = 1;
	
	switch (ecc_request->u16REQ)
	{
		//	1. po ustaleniu parametrów po³¹czenia (wersja SSL, sposób szyfrowania itd.) sprawdzenie podpisu wiadomoœci z serwera
		case ECC_REQ_SIGN_VERIFY:
 		ecc_response.u16Status = ecdsa_process_sign_verify_request(ecc_request->strEcdsaVerifyREQ.u32nSig);
 		break;
		
		
 		case ECC_REQ_CLIENT_ECDH:
		// 2. po zweryfikowaniu podpisu wiadomoœci z serwera wygenerowanie shared secret
		//wygenerowanie nowego klucza publicznego
		
		
		
		
		
		//zerowanie pierwszego znaku klucza????
 		ecc_response.u16Status = ecdh_derive_client_shared_secret(&(ecc_request->strEcdhREQ.strPubKey), ecc_response.strEcdhREQ.au8Key, &ecc_response.strEcdhREQ.strPubKey);
		 
 		break;
// 
 		case ECC_REQ_GEN_KEY:
		asm("BKPT");
// 		ecc_response.u16Status = ecdh_derive_key_pair(&ecc_response.strEcdhREQ.strPubKey);
 		break;
// 
 		case ECC_REQ_SERVER_ECDH:
		asm("BKPT");
// 		ecc_response.u16Status = ecdh_derive_server_shared_secret(ecc_request->strEcdhREQ.strPubKey.u16PrivKeyID,
// 		&(ecc_request->strEcdhREQ.strPubKey),
// 		ecc_response.strEcdhREQ.au8Key);
 		break;
// 		

 		case ECC_REQ_SIGN_GEN:
		asm("BKPT");
// 		ecc_response.u16Status = ecdsa_process_sign_gen_request(&(ecc_request->strEcdsaSignREQ), signature,
// 		&response_data_size);
// 		response_data_buffer = signature;
 		break;
		
		default:
		// Do nothing
		break;
	}
	
	ecc_response.u16REQ      = ecc_request->u16REQ;
	ecc_response.u32UserData = ecc_request->u32UserData;
	ecc_response.u32SeqNo    = ecc_request->u32SeqNo;

	m2m_ssl_ecc_process_done();
	m2m_ssl_handshake_rsp(&ecc_response, response_data_buffer, response_data_size);
}





static void wifi_ssl_callback(uint8 u8MsgType, void *pvMsg)
{
	tstrEccReqInfo *ecc_request = NULL;
	
	switch (u8MsgType)
	{
		case M2M_SSL_REQ_ECC:
		ecc_request = (tstrEccReqInfo*)pvMsg;
// 		printf("%s", (char *)pvMsg);
// 		asm("BKPT");
		
		ecc_process_request(ecc_request);
		break;
		
		case M2M_SSL_RESP_SET_CS_LIST:
		default:
		// Do nothing
		// Przy przetwarzaniu ¿¹dania certyfikatu Root CA odczytaj certyfikaty z pamiêci 
		break;
	}
}

/** Wi-Fi status variable. */
bool receivedTime = false;
/**
 * @brief Default MQTT HOST URL is pulled from the aws_iot_config.h
 */
char HostAddress[255] = AWS_IOT_MQTT_HOST;

/**
 * @brief Default MQTT port is pulled from the aws_iot_config.h
 */
uint32_t port = AWS_IOT_MQTT_PORT;

/**
 * @brief This parameter will avoid infinite loop of publish and exit the program after certain number of publishes
 */
uint32_t publishCount = 0;

/**
 * \brief SysTick handler used to measure precise delay. 
 */

volatile uint32_t ms_ticks = 0;


void SysTick_Handler(void)
{
	ms_ticks++;
}

//callback wywo³ywany przy odebraniu wiadomoœci na zasubskrybowanym temacie - tutaj zawsze lista kart
static int32_t MQTTcallbackHandler(MQTTCallbackParams params) {

	
//	printf("Response received");
	
	parse_cards_list(params.MessageParams.pPayload);
	mqtt_close_flag = true;

	return 0;
}

//funkcja obs³uguj¹ca roz³¹czenie z serwerem
static void disconnectCallbackHandler(void) {
	//printf("MQTT Disconnect");
	IoT_Error_t rc = NONE_ERROR;
	if(aws_iot_is_autoreconnect_enabled()){
		//printf("Auto Reconnect is enabled, Reconnecting attempt will start now");
		}else{
		//printf("Auto Reconnect not enabled. Starting manual reconnect...");
		rc = aws_iot_mqtt_attempt_reconnect();
		if(RECONNECT_SUCCESSFUL == rc){
			//printf("Manual Reconnect Successful%c", ETX);
			}else{
			//printf("Manual Reconnect Failed - %d%c", rc, ETX);
		}
	}
}


//funkcja uruchomienia MQTT
void aws_mqtt_init(void)
{
	
	memset(&wifi_ssid, 0, SSID_LENGTH+1);
	EEPROM_Read(SSID_ADDRESS, wifi_ssid, SSID_LENGTH);								//pobranie SSID z EEPROMu
	memset(&wifi_pass, 0, PASSWORD_LENGTH+1);
	EEPROM_Read(PASSWORD_ADDRESS, wifi_pass, PASSWORD_LENGTH);						//pobranie has³a z EEPROMu
	if (wifi_pass[0] == 255 || wifi_ssid[0] == 255) return;							//jeœli has³o/SSID nie by³y skonfigurowane - przerwij, usuniêcie flagi i wys³anie polecenia w mainie (tutaj nie dzia³a timer)
		
	
	//skopiowane z ASFa
	tstrWifiInitParam param;
	int8_t ret;
	//int32_t i = 0;
	connectParams = MQTTConnectParamsDefault;
	subParams = MQTTSubscribeParamsDefault;
	Msg = MQTTMessageParamsDefault;
	Params = MQTTPublishParamsDefault;
	connectParams.KeepAliveInterval_sec = 10;
	connectParams.isCleansession = true;
	connectParams.MQTTVersion = MQTT_3_1_1;
	connectParams.pClientID = (char*)CLIENT_ID;
	connectParams.pHostURL = HostAddress;
	connectParams.port = port;
	connectParams.isWillMsgPresent = false;
	connectParams.pRootCALocation = NULL;
	connectParams.pDeviceCertLocation = NULL;
	connectParams.pDevicePrivateKeyLocation = NULL;
	connectParams.mqttCommandTimeout_ms = 5000;
	connectParams.tlsHandshakeTimeout_ms = 5000;
	connectParams.isSSLHostnameVerify = true; // ensure this is set to true for production
	connectParams.disconnectHandler = disconnectCallbackHandler;
	
	delay_init();
	
	/* Enable SysTick interrupt for non busy wait delay. */
	if (SysTick_Config(system_cpu_clock_get_hz() / 1000)) {
		//puts("main: SysTick configuration error!");
		//printf("%c", ETX);
		while(1);
	}

	/* Initialize Wi-Fi parameters structure. */
	memset((uint8_t *)&param, 0, sizeof(tstrWifiInitParam));

	/* Initialize Wi-Fi driver with data and status callbacks. */
	param.pfAppWifiCb = wifi_cb;
	ret = m2m_wifi_init(&param);
	if (M2M_SUCCESS != ret) {
		//printf("main: m2m_wifi_init call error!(%d)\r\n%c", ret, ETX);
		m2m_wifi_deinit(NULL);
		uart_send_buffer((uint8_t *)"w-error 04");
		return;
	}
	
	
	// zadeklarowanie przerwania do obs³ugi TLS - tam odes³anie do konkretnego slotu ATECCA w celu podpisania wiadomoœci i wygenerowania shared secret itd.
	ret = m2m_ssl_init(wifi_ssl_callback);
	if (M2M_SUCCESS != ret) {
		while(1) {
		}
	}

	//ograniczenie u¿ywanych certyfikatów do tych korzystaj¹cych z ECC (koniecznoœæ wykorzystania ATECCa i wgrania Root CA ECC np. Amazon Root CA 3)	
	ret = m2m_ssl_set_active_ciphersuites(SSL_ECC_ONLY_CIPHERS);
	if (ret != M2M_SUCCESS)
	{
		while(1) {
		}
	}
	
	
	//po³¹czenie z wifi, parametry pobrane na pocz¹tku funkcji
	m2m_wifi_connect((char *)wifi_ssid, strlen((char *)wifi_ssid), M2M_WIFI_SEC_WPA_PSK, (char *)wifi_pass, M2M_WIFI_CH_ALL);
}

//funkcja wykonywana w pêtli while - obs³uga zdarzeñ MQTT
void aws_main_handler(void)
{
	IoT_Error_t rc = NONE_ERROR;
	char cPayload[100];
	if (mqtt_connected_flag == false)
	{
		m2m_wifi_handle_events(NULL);
		if(gbConnectedWifi && receivedTime)
		{
			//sslEnableCertExpirationCheck(0);
			//printf("Connecting...");
			rc = aws_iot_mqtt_connect(&connectParams);
			if (NONE_ERROR != rc) {
				//printf("Error(%d) connecting to %s:%d%c", rc, connectParams.pHostURL, connectParams.port, ETX);
				
				//problem z po³¹czeniem - reset ustawieñ mqtt, disconnect, roz³¹czenie wi-fi
				send_command_check("w-error 09");
				mqtt_close_flag = true;
			}
		
			/*
			* Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
			*  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
			*  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
			*/
			rc = aws_iot_mqtt_autoreconnect_set_status(false);
			if (NONE_ERROR != rc) {
				//problem z automatycznym odnowieniem po³¹czenia - reset ustawieñ, disconnect, roz³¹czenie wi-fi
				send_command_check("w-error 09");
				mqtt_close_flag = true;
			}
		
			
			subParams.mHandler = MQTTcallbackHandler;
			char subscribe_channel[SERIAL_LENGTH + 18];													//tablica przechowuj¹ca temat do subskrypcji przez MQTT budowany na podstawie numeru seryjnego urz¹dzenia
			memset(&serial_num, 0, SERIAL_LENGTH+1);													//pobranie numeru seryjnego z EEPROMu
			EEPROM_Read(SERIAL_ADDRESS, serial_num, SERIAL_LENGTH);
			
			
			
			
			//dodaæ warunek dla mqtt_subscribe_command ze zmian¹ tematu po ustawieniu lambdy
			sprintf(subscribe_channel, "$aws/things/%s/state", (char *)serial_num);						//ustalenie tematu do subskrypcji
			
			
			
			
			
			subParams.pTopic = (char*)subscribe_channel;												//tutaj ustawiæ adres dla transactions
			subParams.qos = QOS_0;
			if ((command_pointer == mqtt_connect_command) || (command_pointer == list_download_command) || (command_pointer == mqtt_subscribe_command))		//sprawdzenie ostatnio u¿ytej funkcji
			{
				if (NONE_ERROR == rc) {
					rc = aws_iot_mqtt_subscribe(&subParams);				//subskrypcja tematu ustalonego wy¿ej
					if (NONE_ERROR != rc) {
						//znowu reset ustawieñ i disconnect, roz³¹czenie wi-fi
						send_command_check("w-error 09");
						mqtt_close_flag = true;
					}
				}
				mqtt_connected_flag = true;												//flaga wystawiona po ustabilizowaniu po³¹czenia przez MQTT
				Msg.qos = QOS_0;
				Msg.pPayload = (void *) cPayload;
				
				char publish_channel[SERIAL_LENGTH + 26];										//ustalenie tematu, na który wysy³ana jest wiadomoœæ oznaczaj¹ca chêæ pobrania stanu "thinga" (listy kart)
				
				sprintf(publish_channel, "$aws/things/%s/shadow/get", (char *)serial_num);
				
				Params.pTopic = (char*)publish_channel;
				if (publishCount != 0) {
					infinitePublishFlag = false;
				}
				
				
				//wys³anie pustej wiadomoœci, odpowiedŸ powinna pojawiæ siê na subskrybowanym temacie (specyfika get state z AWSa)
				sprintf(cPayload, "");
				Msg.PayloadLen = 0;
				Params.MessageParams = Msg;
				rc = aws_iot_mqtt_publish(&Params);
			}
		}
	}
	else if (publishCount > 0 || infinitePublishFlag)
	{
		rc = aws_iot_mqtt_yield(100);						//obs³uga zdarzeñ MQTT
		if(NETWORK_ATTEMPTING_RECONNECT == rc){
			//printf("-->sleep reconnect\r\n%c", ETX);
			delay_ms(1);
			// If the client is attempting to reconnect we will skip the rest of the loop.
			//return;
		}
	}
}

//zakoñczenie po³¹czenia przez MQTT
void aws_mqtt_deinit(void)
{
	aws_iot_mqtt_disconnect();		//roz³¹czenie
	m2m_wifi_deinit(NULL);			//wy³¹czenie ATWINCa
	gbSocketInit = 0;				//usuniêcie flag po³¹czenia
	gbConnectedWifi = false;
	mqtt_flag = false;
}