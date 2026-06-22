#include "i2c.h"

I2C_HandleTypeDef hi2c1;

/* I2C1 init: PB8=SCL, PB9=SDA, 100kHz, remap enabled */
void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
    {
        Error_Handler();
    }
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *i2cHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if (i2cHandle->Instance == I2C1)
    {
        __HAL_RCC_GPIOB_CLK_ENABLE();

        /* PB8=SCL, PB9=SDA: open-drain AF */
        GPIO_InitStruct.Pin = SCL_Pin | SDA_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        /* Remap I2C1 from PB6/PB7 to PB8/PB9 */
        __HAL_AFIO_REMAP_I2C1_ENABLE();

        __HAL_RCC_I2C1_CLK_ENABLE();
    }
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef *i2cHandle)
{
    if (i2cHandle->Instance == I2C1)
    {
        __HAL_RCC_I2C1_CLK_DISABLE();
        HAL_GPIO_DeInit(SCL_GPIO_Port, SCL_Pin);
        HAL_GPIO_DeInit(SDA_GPIO_Port, SDA_Pin);
    }
}
