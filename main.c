#include "xparameters.h"
#include "xil_printf.h"
#include "sleep.h"
#include "stdbool.h"
#include "PmodESP32.h"
#include "cJSON/cJSON.h"
#include "stdio.h"
#include "xtime_l.h"
#include "xgpio.h"

#define HOST_UART_DEVICE_ID XPAR_PS7_UART_1_DEVICE_ID
#define HostUart XUartPs
#define HostUart_Config XUartPs_Config
#define HostUart_CfgInitialize XUartPs_CfgInitialize
#define HostUart_LookupConfig XUartPs_LookupConfig
#define HostUart_Recv XUartPs_Recv
#define HostUartConfig_GetBaseAddr(CfgPtr) (CfgPtr->BaseAddress)
#include "xuartps.h"

#define PMODESP32_UART_BASEADDR XPAR_PMODESP32_0_AXI_LITE_UART_BASEADDR
#define PMODESP32_GPIO_BASEADDR XPAR_PMODESP32_0_AXI_LITE_GPIO_BASEADDR

#define COUNTS_PER_SECOND	(XPAR_CPU_CORTEXA9_CORE_CLOCK_FREQ_HZ /2)

void EnableCaches();
void DisableCaches();
void DemoInitialize();
void DemoRun();
void DemoCleanup();

PmodESP32 myESP32;
HostUart myHostUart;
char *jsonStr;
u8 postRequest[360];
int countdown = 60;

void DemoInitialize () {
	HostUart_Config *CfgPtr;
	EnableCaches();
	ESP32_Initialize(&myESP32, PMODESP32_UART_BASEADDR, PMODESP32_GPIO_BASEADDR);
	CfgPtr = HostUart_LookupConfig(HOST_UART_DEVICE_ID);
	HostUart_CfgInitialize(&myHostUart, CfgPtr, HostUartConfig_GetBaseAddr(CfgPtr));
}

/*Uses cJSON library to build JSON string
 Format:
 "api_key": "XXXXXXX",
    "to": ["Test Person <test@example.com>"],
    "sender": "<test2@example.com>",
    "subject": "Hello Test Person",
    "text_body": "You're my favorite test person ever",
 */
void createJSON(){
	cJSON *json = cJSON_CreateObject();
	cJSON_AddStringToObject(json, "api_key", "api-18A36564BEA24C619218D21314B09AF9");
	cJSON *email = cJSON_CreateString("8565202345@txt.att.net");
	cJSON *to = cJSON_CreateArray();
	cJSON_AddItemToArray(to, email);
	cJSON_AddItemToObject(json,"to", to);
	cJSON_AddStringToObject(json, "sender", "testwjwd@gmail.com");
	cJSON_AddStringToObject(json, "subject", "Wet Window!");
	cJSON_AddStringToObject(json, "text_body", "Close your window stupid");

	jsonStr = cJSON_Print(json);
}

/*
 * Builds HTTP POST request payload. Each line has to have \r\n.
 * Hard coded to send to api.smtp2go.com
 * Format:
POST /v3/email/send HTTP/1.1
Host: api.smtp2go.com
Content-Type: application/json
Content-Length: 192

{cJSON object string}
 */
void createHTTP(void){
	createJSON();
	sprintf((char*)postRequest, "POST /v3/email/send HTTP/1.1\r\nHost: api.smtp2go.com\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n",strlen((char*)jsonStr));
	//Appends cJSON object, must have extra \r\n before appending this
	strncat((char*)postRequest,jsonStr, strlen(jsonStr));
}

/*
 * Helper function to receive and print data from ESP given certain time duration
 * Also helpful to force program to "wait" for ESP to execute commands
 */
void receiveData(XTime time){
	XTime tEnd, tCur;
	u8 recv_buffer=0;
	u32 num_received=0;

	XTime_GetTime(&tCur);
	tEnd  = tCur + (time * COUNTS_PER_SECOND);
	do
    {
		num_received = ESP32_Recv(&myESP32, &recv_buffer,1);
				if(num_received >0){
					num_received = ESP32_Recv(&myESP32, &recv_buffer,1);
					xil_printf("%c", recv_buffer);
				}
		if(tCur == tCur + COUNTS_PER_SECOND){
			countdown = countdown -1;
		}
		else
			XTime_GetTime(&tCur);
    } while (tCur < tEnd);

}

/*
 * Establishes SSL connection with smtp2go API
 * Connection must be SSL on port 443 since API only accepts calls over HTTPS
 */
void establishConnection(void){
	//Build AT command
	u8 tx[] = "AT+CIPSTART=\"SSL\",\"api.smtp2go.com\",443\r\n";
	u32 num = strlen((char *) tx);
	xil_printf((char *) tx);
	//Send payload and message len
	ESP32_SendBuffer(&myESP32, tx, num);
	receiveData(10);
}

/*
 * Sends AT command to send data over SSL connection
 * CIPSEND with POST length gets sent first, then function wait some time for ESP to display ">"
 * ESP reads serial data coming in up to POST length, then sends payload
 */
void sendPost(void){
	u8 cmd[50];
	//Building AT command with POST length
	sprintf((char*)cmd,"AT+CIPSEND=%d\r\n", strlen((char*)postRequest));
	xil_printf((char*) cmd);

	xil_printf("%d\r\n", strlen((char*)postRequest));
	//Sending AT command to ESP
	ESP32_SendBuffer(&myESP32, cmd, strlen((char*)cmd));
	//Primarily serves as a "Wait", as we can't send request data until ESP processes command and shows ">"
	receiveData(3);

	xil_printf((char*)postRequest);
	//Send POST payload, ESP automatically sends data once POST length is reached
	ESP32_SendBuffer(&myESP32, postRequest, strlen((char*)postRequest));
	//Wait for response from API server
	receiveData(30);
}


int main(){
	XGpio waterSensor;
	u32 data;
	bool waterDetected = false;
	int waterCount = 0;

	DemoInitialize();
	xil_printf("Prepping HTTP load..\r\n");
	createHTTP();

	xil_printf("Initializing water sensor..\r\n");
	//Setting up external GPIO pin for water sensor, uses pin 1 on PMOD port JC
	XGpio_Initialize(&waterSensor, XPAR_PS7_GPIO_0_DEVICE_ID);
	//Sets data as input
	XGpio_SetDataDirection(&waterSensor, 1, 0xF);

	xil_printf("Setup complete, standing by for water detection!\r\n");

	while(!waterDetected){
		//Signal from water sensor acts as just HIGH or LOW, so we just need to read 1s or 0s
		data = XGpio_DiscreteRead(&waterSensor, 1);
		waterCount = waterCount + data;
		if(data){
			xil_printf("%d,",waterCount);
		}

		//Sensor is pretty sensitive, so creating slight counter to not have false positives
		if(waterCount > 100){
			xil_printf("Water detected!\r\n");
			//Break while loop so we don't get a million emails
			waterDetected = true;

			xil_printf("Establishing connection with mail server..\r\n");
			establishConnection();

			xil_printf("Sending payload to server.. \r\n");
			sendPost();
		}
	}

	xil_printf("User notified! Ending service \r\n");
	return 0;
}

void DemoCleanup() {
	DisableCaches();
}

void EnableCaches() {
#ifdef __MICROBLAZE__
#ifdef XPAR_MICROBLAZE_USE_DCACHE
   Xil_DCacheEnable();
#endif
#ifdef XPAR_MICROBLAZE_USE_ICACHE
   Xil_ICacheEnable();
#endif
#endif
}

void DisableCaches() {
#ifdef __MICROBLAZE__
#ifdef XPAR_MICROBLAZE_USE_DCACHE
   Xil_DCacheDisable();
#endif
#ifdef XPAR_MICROBLAZE_USE_ICACHE
   Xil_ICacheDisable();
#endif
#endif
}
