#include "dht11.h"
#include "delay.h"

static uint8_t data[5];
/*
 * DHT11引脚：输入/输出模式配置函数 
 * Mode = 0/INPUT  时 输入模式  
 * Mode = 1/OUTPUT 时 输出模式  
 */
void DHT11_PIN_Mode(int Mode)
{	
   if(Mode)   
   {
		GPIO_InitTypeDef GPIO_InitStruct = {0};							// 定义GPIO_InitTypeDef结构体 
		GPIO_InitStruct.Pin = DHT11_Pin;                    // 引脚选择
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;         // 引脚模式：输出模式
		GPIO_InitStruct.Pull = GPIO_NOPULL;                 // 配置内部上拉
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;       // 引脚速率：高速
		HAL_GPIO_Init(DHT11_GPIO_Port, &GPIO_InitStruct);
   }
   else
   {
		GPIO_InitTypeDef GPIO_InitStruct = {0};						// 定义GPIO_InitTypeDef结构体 
		GPIO_InitStruct.Pin = DHT11_Pin;				    				// 引脚选择
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;							// 引脚模式：输入模式
		GPIO_InitStruct.Pull = GPIO_NOPULL;									// 配置内部上拉
		HAL_GPIO_Init(DHT11_GPIO_Port, &GPIO_InitStruct);
   }
}  
 
/*
 *DHT11起始函数
 *根据DHT11时序图，主机要要发送起始信号，需要将总线电平拉低（18~30ms）
 */
void DHT11_Start(void)
{
	DHT11_PIN_Mode(OUTPUT);
	DHT11_IO_SET;
	HAL_Delay(1);
	DHT11_IO_RESET;
	HAL_Delay(20);
	DHT11_IO_SET;
	delayus(30);
	DHT11_PIN_Mode(INPUT);   /* release bus so DHT11 can pull it low */
}
 
/**
  * DHT11响应检测函数
  * 返回1：未检测到DHT11的存在
  * 返回0：存在出现由高到低的变化即可
  */
uint8_t DHT11_Check(void)
{
	uint8_t retry = 0;
	DHT11_PIN_Mode(INPUT);              //将引脚切换为输入模式
	while(!DHT11_IO_Read && retry<100)  //单片机发送起始信号后，DHT11会将总线拉低83微妙
	{
		retry++;
		delayus(1);
	}
	if(retry >= 100)return 1;
	else retry = 0;
	
	while(DHT11_IO_Read && retry<100)  //DHT11拉低后会再次拉高87微妙
	{
		retry++;
		delayus(1);
	}
	if(retry >= 100) return 1;
	else return 0;
}
 
 
/**
  * 从DHT11读取一个位
  * 返回值：1/0
  */
uint8_t DHT11_Read_Bit(void)
{
	DHT11_PIN_Mode(INPUT);
	while(!DHT11_IO_Read);
	delayus(40);
	if(DHT11_IO_Read)
	{
		while(DHT11_IO_Read);
		return 1;
	}
	else
	{
		return 0;
	}
}
 
/**
  *  读取一个字节数据 1byte / 8bit
  *  返回值是一个字节的数据
  */
uint8_t DHT11_Read_Byte(void)
{
    uint8_t i,buf = 0;                             //  暂时存储数据
    
    for(i=0; i<8 ;i++)
    {
        buf <<= 1;                                 
        if(DHT11_Read_Bit())                        //  1byte -> 8bit
        {
            buf |= 1;                              //  0000 0001
        }
    }
    return buf;
}
 
/**
  * 读取温湿度传感器数据 5byte / 40bit
  * 使用方法：创建两个float变量，将变量地址传入函数
  * 注意：两次使用该函数的间隔需要大于2秒，否则会导致数据测量不准确
  */
/* Read temperature and humidity from DHT11.
 * Returns: 1=success, 0=checksum error, 2=no response.
 * Minimum 2-second interval between calls.
 * Interrupts are disabled during the bit-banging phase to prevent
 * RTOS preemption from corrupting microsecond-level DHT11 timing. */
uint8_t DHT11_READ_DATA(float *temp, float *humi)
{
   uint8_t i, result;
   DHT11_Start();                               /* 20ms start pulse (HAL_Delay needs IRQ on) */

   __disable_irq();                             /* lock timing for bit-bang */
   if(!DHT11_Check())                           /* DHT11 ACK */
   {
      for(i=0; i<5; i++)
      {
         data[i] = DHT11_Read_Byte();           /* read 5 bytes */
      }

      if(data[0] + data[1] + data[2] + data[3] == data[4])
      {
         *humi = data[0] + 0.1*data[1];
         *temp = data[2] + 0.1*data[3];
         result = 1;                            /* checksum OK */
      }
      else result = 0;                          /* checksum fail */
   }
   else result = 2;                             /* no response */
   __enable_irq();

   return result;
}
 
