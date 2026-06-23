// esp8266_mqtt.h
#ifndef __ESP8266_MQTT_H
#define __ESP8266_MQTT_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"


#define ESP_BUFF_SIZE 512
typedef int32_t ESP_Status;

#ifndef ESP_UART
#define ESP_UART huart1
#endif

bool ESP8266_WaitResponse(const char* token, uint32_t timeout);
void ESP8266_UART_IRQHandler(void);
ESP_Status ESP8266_Init(void);
uint8_t ESP8266_JoinAccessPoint(const char* ssid, const char* pwd);
ESP_Status ESP8266_ConnectToServer(const char* ip, const char* port);
void Esp8266_SendAtNotWaitResponse(uint8_t* str, uint16_t len);
ESP_Status ESP8266_MqttPub(const char* topic, const uint8_t* data, uint16_t len);
ESP_Status ESP8266_MqttUserCfg(const char* clientId, const char* username,const char* password);
ESP_Status ESP8266_MqttConnect(uint8_t idx,
	const char* host, const char* port,
	bool secure);

#endif // __ESP8266_MQTT_H