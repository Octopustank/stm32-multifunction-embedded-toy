#include "dht11.h"
#include "delay.h"

static uint8_t data[5];

void DHT11_PIN_Mode(int Mode)
{
   if(Mode)
   {
		GPIO_InitTypeDef GPIO_InitStruct = {0};
		GPIO_InitStruct.Pin = DHT11_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
		HAL_GPIO_Init(DHT11_GPIO_Port, &GPIO_InitStruct);
   }
   else
   {
		GPIO_InitTypeDef GPIO_InitStruct = {0};
		GPIO_InitStruct.Pin = DHT11_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(DHT11_GPIO_Port, &GPIO_InitStruct);
   }
}

void DHT11_Start(void)
{
	DHT11_PIN_Mode(OUTPUT);
	DHT11_IO_SET;
	HAL_Delay(1);
	DHT11_IO_RESET;
	HAL_Delay(20);
	DHT11_IO_SET;
	delayus(30);
	DHT11_PIN_Mode(INPUT);
}

uint8_t DHT11_Check(void)
{
	uint8_t retry = 0;
	DHT11_PIN_Mode(INPUT);
	while(!DHT11_IO_Read && retry < 100)
	{
		retry++;
		delayus(1);
	}
	if(retry >= 100) return 1;

	retry = 0;
	while(DHT11_IO_Read && retry < 100)
	{
		retry++;
		delayus(1);
	}
	if(retry >= 100) return 1;
	return 0;
}

/* Read one bit with timeout — allows SysTick to interrupt without deadlock */
uint8_t DHT11_Read_Bit(void)
{
	uint16_t retry = 0;
	DHT11_PIN_Mode(INPUT);

	while(!DHT11_IO_Read && retry < 10000) retry++;
	if(retry >= 10000) return 0;

	delayus(40);
	if(DHT11_IO_Read)
	{
		retry = 0;
		while(DHT11_IO_Read && retry < 10000) retry++;
		return 1;
	}
	return 0;
}

uint8_t DHT11_Read_Byte(void)
{
    uint8_t i, buf = 0;
    for(i = 0; i < 8; i++)
    {
        buf <<= 1;
        if(DHT11_Read_Bit())
            buf |= 1;
    }
    return buf;
}

/* Read DHT11 without disabling interrupts.
 * SysTick preemption may corrupt occasional bits;
 * the checksum and high-frequency retry absorb the loss. */
uint8_t DHT11_READ_DATA(float *temp, float *humi)
{
   uint8_t i;
   DHT11_Start();

   if(!DHT11_Check())
   {
      for(i = 0; i < 5; i++)
         data[i] = DHT11_Read_Byte();

      if(data[0] + data[1] + data[2] + data[3] == data[4])
      {
         *humi = data[0] + 0.1f * data[1];
         *temp = data[2] + 0.1f * data[3];
         return 1;
      }
      return 0;
   }
   return 2;
}
