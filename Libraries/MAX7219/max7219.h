#ifndef __MAX7219_H
#define __MAX7219_H

#include "stm32f1xx_hal.h"

/* ---- pin definitions (software SPI) ---- */
#define MAX7219_DIN_PIN    GPIO_PIN_11
#define MAX7219_DIN_PORT   GPIOA
#define MAX7219_CLK_PIN    GPIO_PIN_12
#define MAX7219_CLK_PORT   GPIOA
#define MAX7219_CS_PIN     GPIO_PIN_15
#define MAX7219_CS_PORT    GPIOA

/* ---- registers ---- */
#define MAX7219_REG_NOOP        0x00
#define MAX7219_REG_DIGIT0      0x01
#define MAX7219_REG_DIGIT1      0x02
#define MAX7219_REG_DIGIT2      0x03
#define MAX7219_REG_DIGIT3      0x04
#define MAX7219_REG_DIGIT4      0x05
#define MAX7219_REG_DIGIT5      0x06
#define MAX7219_REG_DIGIT6      0x07
#define MAX7219_REG_DIGIT7      0x08
#define MAX7219_REG_DECODE      0x09
#define MAX7219_REG_INTENSITY   0x0A
#define MAX7219_REG_SCANLIMIT   0x0B
#define MAX7219_REG_SHUTDOWN    0x0C
#define MAX7219_REG_DISPTEST    0x0F

void max7219_init(void);
void max7219_write(uint8_t addr, uint8_t data);
void max7219_display(const uint8_t pattern[8]);

#endif
