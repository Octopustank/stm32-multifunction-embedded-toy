#ifndef __DHT11_H
#define __DHT11_H

#include "stm32f1xx_hal.h"


#define DHT11_GPIO_Port GPIOB
#define DHT11_Pin       GPIO_PIN_5
 
#define DHT11_IO_Read   HAL_GPIO_ReadPin(DHT11_GPIO_Port,DHT11_Pin)                   //读DHT11引脚电平
#define DHT11_IO_SET    HAL_GPIO_WritePin(DHT11_GPIO_Port,DHT11_Pin,GPIO_PIN_SET)     //DHT11引脚置高电平
#define DHT11_IO_RESET  HAL_GPIO_WritePin(DHT11_GPIO_Port,DHT11_Pin,GPIO_PIN_RESET)   //DHT11引脚置低电平
  
#define INPUT 0   //引脚输入模式
#define OUTPUT 1  //引脚输出模式
 
void DHT11_Delay_us(__IO uint32_t delay);           // 微妙级延时函数
void DHT11_PIN_Mode(int Mode);  										// 引脚模式配置函数
void DHT11_Start(void);         										// 起始信号发送函数
uint8_t DHT11_Check(void);      										// DHT11应答检测函数
uint8_t DHT11_Read_Bit(void);   										// 读取一个数据位（bit），8 bit = 1 byte
uint8_t DHT11_Read_Byte(void);  										// 读取一个字节的数据
uint8_t DHT11_READ_DATA(float *temp, float *humi);  // 温湿度数据读取函数
 
#endif
