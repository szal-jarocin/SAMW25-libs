/**
 * \file
 *
 * \brief MAIN configuration.
 *
 * Copyright (c) 2016-2018 Microchip Technology Inc. and its subsidiaries.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Subject to your compliance with these terms, you may use Microchip
 * software and any derivatives exclusively with Microchip products.
 * It is your responsibility to comply with third party license terms applicable
 * to your use of third party software (including open source software) that
 * may accompany Microchip software.
 *
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE,
 * INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY,
 * AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT WILL MICROCHIP BE
 * LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, INCIDENTAL OR CONSEQUENTIAL
 * LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND WHATSOEVER RELATED TO THE
 * SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS BEEN ADVISED OF THE
 * POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.  TO THE FULLEST EXTENT
 * ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY
 * RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
 * THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *
 * \asf_license_stop
 *
 */

#ifndef MAIN_H_INCLUDED
#define MAIN_H_INCLUDED


/** Wi-Fi status variable. */
//bool gbConnectedWifi;

/** Get host IP status variable. */
//bool gbHostIpByName;

extern void configure_console(void);

enum Console_Res_t {
	Console_OK = 0,													// Zwracane przez wszystkie funkcje, je¿eli zakoñczy³y siê prawid³owo
	Console_ReceivedCommand,										// Console_UartInput() - odebrano pe³ne polecenie gotowe do dalszej analizy
	Console_ReceivedRetry,
	Console_ReceivedENQ,
	Console_ReceivedACK,
	Console_ReceiveError,
	Console_ReceivedCRC,
// 	Console_ReceivedData,											// Console_UartInput() - odebrano dane zakoñczone znakiem ETX
// 	Console_HostReady,												// Console_UartInput() - otrzymano od hosta znak DC4, mo¿na wys³aæ nastêpna komendê
 	Console_BufferFull,												// Console_UartInput() - przepe³nienie bufora
// 	Console_InputCancelled,											// Console_UartInput() - wciœniêto ESC, trzeba wyœwietliæ ponownie znak zachêty wiersza poleceñ
	Console_ErrorToFix = 255,										// Tylko do celów deweloperskich, normalnie ¿adna funkcja nie powinna zwracaæ czegoœ takiego
};

#ifdef __cplusplus
extern "C" {
	#endif


	typedef enum {
		SocketInit = 0,
		SocketConnect,
		SocketWaiting,
		SocketComplete,
		SocketError
	} eSocketStatus;

	#ifdef __cplusplus
}
#endif

#endif /* MAIN_H_INCLUDED */
