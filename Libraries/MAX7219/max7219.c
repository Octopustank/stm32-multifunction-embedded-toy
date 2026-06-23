#include "max7219.h"

static void max7219_send16(uint8_t addr, uint8_t data)
{
    uint16_t word = ((uint16_t)addr << 8) | data;

    MAX7219_CS_PORT->BRR = MAX7219_CS_PIN;

    for (int i = 15; i >= 0; i--) {
        MAX7219_CLK_PORT->BRR = MAX7219_CLK_PIN;
        if (word & (1 << i))
            MAX7219_DIN_PORT->BSRR = MAX7219_DIN_PIN;
        else
            MAX7219_DIN_PORT->BRR  = MAX7219_DIN_PIN;
        __NOP(); __NOP();
        MAX7219_CLK_PORT->BSRR = MAX7219_CLK_PIN;
        __NOP(); __NOP();
    }

    MAX7219_CS_PORT->BSRR = MAX7219_CS_PIN;
}

void max7219_write(uint8_t addr, uint8_t data)
{
    max7219_send16(addr, data);
}

void max7219_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIOA->BSRR = MAX7219_CS_PIN | MAX7219_CLK_PIN;

    GPIO_InitTypeDef s = {0};
    s.Mode  = GPIO_MODE_OUTPUT_PP;
    s.Pull  = GPIO_NOPULL;
    s.Speed = GPIO_SPEED_FREQ_HIGH;

    s.Pin = MAX7219_CS_PIN;
    HAL_GPIO_Init(MAX7219_CS_PORT, &s);
    s.Pin = MAX7219_DIN_PIN;
    HAL_GPIO_Init(MAX7219_DIN_PORT, &s);
    s.Pin = MAX7219_CLK_PIN;
    HAL_GPIO_Init(MAX7219_CLK_PORT, &s);

    HAL_Delay(100);

    max7219_send16(0x09, 0x00);
    max7219_send16(0x0A, 0x08);
    max7219_send16(0x0B, 0x07);
    max7219_send16(0x0C, 0x01);
    max7219_send16(0x0F, 0x00);
}

void max7219_display(const uint8_t pattern[8])
{
    for (int i = 0; i < 8; i++)
        max7219_send16(0x01 + i, pattern[i]);
}
