#include "max7219.h"

static void delay_sw(void)
{
    for (volatile int i = 0; i < 4; i++) __NOP();
}

static void max7219_send16(uint8_t addr, uint8_t data)
{
    uint16_t word = ((uint16_t)addr << 8) | data;

    HAL_GPIO_WritePin(MAX7219_CS_PORT, MAX7219_CS_PIN, GPIO_PIN_RESET);

    for (int i = 15; i >= 0; i--) {
        HAL_GPIO_WritePin(MAX7219_CLK_PORT, MAX7219_CLK_PIN, GPIO_PIN_RESET);
        if (word & (1 << i))
            HAL_GPIO_WritePin(MAX7219_DIN_PORT, MAX7219_DIN_PIN, GPIO_PIN_SET);
        else
            HAL_GPIO_WritePin(MAX7219_DIN_PORT, MAX7219_DIN_PIN, GPIO_PIN_RESET);
        delay_sw();
        HAL_GPIO_WritePin(MAX7219_CLK_PORT, MAX7219_CLK_PIN, GPIO_PIN_SET);
        delay_sw();
    }

    HAL_GPIO_WritePin(MAX7219_CS_PORT, MAX7219_CS_PIN, GPIO_PIN_SET);
}

void max7219_write(uint8_t addr, uint8_t data)
{
    max7219_send16(addr, data);
}

void max7219_init(void)
{
    GPIO_InitTypeDef s = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    s.Mode  = GPIO_MODE_OUTPUT_PP;
    s.Pull  = GPIO_NOPULL;
    s.Speed = GPIO_SPEED_FREQ_HIGH;

    s.Pin = MAX7219_DIN_PIN;
    HAL_GPIO_Init(MAX7219_DIN_PORT, &s);
    s.Pin = MAX7219_CLK_PIN;
    HAL_GPIO_Init(MAX7219_CLK_PORT, &s);
    s.Pin = MAX7219_CS_PIN;
    HAL_GPIO_Init(MAX7219_CS_PORT, &s);

    HAL_GPIO_WritePin(MAX7219_CS_PORT, MAX7219_CS_PIN, GPIO_PIN_SET);

    max7219_send16(MAX7219_REG_SHUTDOWN,  0x00);  /* shutdown during config */
    max7219_send16(MAX7219_REG_DISPTEST,  0x00);  /* normal mode */
    max7219_send16(MAX7219_REG_SCANLIMIT, 0x07);  /* all 8 digits */
    max7219_send16(MAX7219_REG_DECODE,    0x00);  /* no BCD decode */
    max7219_send16(MAX7219_REG_INTENSITY, 0x02);  /* low brightness */
    max7219_send16(MAX7219_REG_SHUTDOWN,  0x01);  /* normal operation */
}

void max7219_display(const uint8_t pattern[8])
{
    for (int i = 0; i < 8; i++)
        max7219_send16(MAX7219_REG_DIGIT1 + i, pattern[7 - i]);
}
