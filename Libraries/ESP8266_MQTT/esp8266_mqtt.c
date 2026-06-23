// esp8266_mqtt.c
#include "esp8266_mqtt.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>

static uint8_t uart_buf[ESP_BUFF_SIZE];
static volatile uint16_t uart_idx;
static volatile uint32_t last_rx_tick;
static uint8_t esp_rx_byte;

#define WAIT_TIMEOUT_MS(ms,start) ((HAL_GetTick()-start)<ms)

void ESP8266_UART_IRQHandler(void)
{
	last_rx_tick = HAL_GetTick( );
	if (uart_idx < ESP_BUFF_SIZE - 1)uart_buf[uart_idx++] = esp_rx_byte;
	uart_buf[uart_idx] = '\0';
	HAL_UART_Receive_IT(&ESP_UART, &esp_rx_byte, 1);
}

ESP_Status ESP8266_Init(void)
{
	uart_idx = 0; memset(uart_buf, 0, ESP_BUFF_SIZE);
	HAL_UART_Receive_IT(&ESP_UART, &esp_rx_byte, 1);
	HAL_Delay(500);
	Esp8266_SendAtNotWaitResponse((uint8_t*) "AT\r\n",4);
	if (!ESP8266_WaitResponse("OK", 2000))return-2;
	Esp8266_SendAtNotWaitResponse((uint8_t*) "ATE0\r\n", 6);
	if (!ESP8266_WaitResponse("OK", 2000))return-2;
	Esp8266_SendAtNotWaitResponse((uint8_t*) "AT+RST\r\n", 8);
	return ESP8266_WaitResponse("OK", 2000) ? 0 : -2;
}

uint8_t ESP8266_JoinAccessPoint(const char* ssid, const char* pwd)
{
	uart_idx = 0; memset(uart_buf, 0, ESP_BUFF_SIZE);
	char cmd[128]; snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, pwd);
	Esp8266_SendAtNotWaitResponse((uint8_t*) cmd, strlen(cmd));
	if (ESP8266_WaitResponse("WIFI CONNECTED", 15000))return 0;
	if (strstr((char*) uart_buf, "+CWJAP:1"))return 1;
	if (strstr((char*) uart_buf, "+CWJAP:3"))return 3;
	if (strstr((char*) uart_buf, "+CWJAP:2"))return 2;
	return 4;
}

ESP_Status ESP8266_ConnectToServer(const char* ip, const char* port)
{
	uart_idx = 0; memset(uart_buf, 0, ESP_BUFF_SIZE);
	char cmd[128]; snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%s\r\n", ip, port);
	Esp8266_SendAtNotWaitResponse((uint8_t*) cmd, strlen(cmd));
//	return ESP8266_WaitResponse("CONNECT", 10000) ? 0 : -1;
	return ESP8266_WaitResponse("OK", 10000) ? 0 : -1;
}

void Esp8266_SendAtNotWaitResponse(uint8_t* str, uint16_t len)
{
	HAL_UART_Transmit(&ESP_UART, str, len, HAL_MAX_DELAY);
}

ESP_Status ESP8266_MqttUserCfg(const char* clientId,
	const char* username, const char* password)
{
	char cmd[192]; snprintf(cmd, sizeof(cmd),
		"AT+MQTTUSERCFG=%d,%d,\"%s\",\"%s\",\"%s\",%u,%d,\"%s\"\r\n",0, 1, clientId, username, password, 0, 0, "");
	Esp8266_SendAtNotWaitResponse((uint8_t*) cmd, strlen(cmd));
	return ESP8266_WaitResponse("OK", 10000) ? 0 : -1;
}

ESP_Status ESP8266_MqttConnect(uint8_t idx, const char* host, const char* port, bool secure)
{
	char cmd[128]; snprintf(cmd, sizeof(cmd),
		"AT+MQTTCONN=%d,\"%s\",%s,%d\r\n", idx, host, port, secure ? 1 : 0);
	Esp8266_SendAtNotWaitResponse((uint8_t*) cmd, strlen(cmd));
	return ESP8266_WaitResponse("OK", 5000) ? 0 : -1;
}

bool ESP8266_WaitResponse(const char* token, uint32_t timeout)
{
	uint32_t start = HAL_GetTick( );
	while (WAIT_TIMEOUT_MS(timeout, start)) {
		if (( HAL_GetTick( ) - last_rx_tick ) > 50 && uart_idx) {
			if (strstr((char*) uart_buf, token)) {
				uart_idx = 0; memset(uart_buf, 0, ESP_BUFF_SIZE);
				return true;
			}
		}
	}
	return false;
}
