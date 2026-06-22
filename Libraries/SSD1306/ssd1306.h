#ifndef _SSD1306_H
#define _SSD1306_H

#include "stm32f1xx_hal.h"
#include "fonts.h"

/* 
 * 文件名：ssd1306.h
 * 功能：用于操作 SSD1306 OLED 显示屏的驱动程序接口。
 * 提供初始化、像素绘制、字符串写入等功能。
 */

// I2C 地址（默认值为 0x78，对应 7 位地址 0x3C 左移一位）
// 如果屏幕地址与此不同，请修改定义
#ifndef SSD1306_I2C_ADDR
#define SSD1306_I2C_ADDR        0x78
#endif // SSD1306_I2C_ADDR

// 屏幕宽度（默认 128 像素）
#ifndef SSD1306_WIDTH
#define SSD1306_WIDTH           128
#endif // SSD1306_WIDTH

// 屏幕高度（默认 64 像素）
#ifndef SSD1306_HEIGHT
#define SSD1306_HEIGHT          64
#endif // SSD1306_HEIGHT

// 是否使用 COM 引脚左右翻转（默认禁用）
#ifndef SSD1306_COM_LR_REMAP
#define SSD1306_COM_LR_REMAP    0
#endif // SSD1306_COM_LR_REMAP

// 是否使用 COM 引脚替代引脚配置（默认启用）
#ifndef SSD1306_COM_ALTERNATIVE_PIN_CONFIG
#define SSD1306_COM_ALTERNATIVE_PIN_CONFIG    1
#endif // SSD1306_COM_ALTERNATIVE_PIN_CONFIG

//
// 屏幕颜色枚举
//
typedef enum {
    Black = 0x00,   // 黑色（像素关闭）
    White = 0x01,   // 白色（像素开启）
} SSD1306_COLOR;

//
// 存储屏幕转换状态的结构体
//
typedef struct {
    uint16_t CurrentX;    // 当前 X 坐标
    uint16_t CurrentY;    // 当前 Y 坐标
    uint8_t Inverted;     // 是否反色显示
    uint8_t Initialized;  // 是否已初始化
} SSD1306_t;

//
// 函数声明
//

/**
 * @brief 初始化 SSD1306 显示屏。
 * @param hi2c I2C 句柄。
 * @return 操作状态（1 = 成功，0 = 失败）。
 */
uint8_t ssd1306_Init(I2C_HandleTypeDef *hi2c);

/**
 * @brief 更新屏幕内容。
 * @param hi2c I2C 句柄。
 */
void ssd1306_UpdateScreen(I2C_HandleTypeDef *hi2c);

/**
 * @brief 用指定颜色填充屏幕。
 * @param color 填充颜色（Black 或 White）。
 */
void ssd1306_Fill(SSD1306_COLOR color);

/**
 * @brief 在屏幕指定位置绘制一个像素。
 * @param x X 坐标。
 * @param y Y 坐标。
 * @param color 像素颜色。
 */
void ssd1306_DrawPixel(uint8_t x, uint8_t y, SSD1306_COLOR color);

/**
 * @brief 写入一个字符到屏幕。
 * @param ch 字符。
 * @param Font 字体定义。
 * @param color 字体颜色。
 * @return 操作状态（1 = 成功，0 = 失败）。
 */
char ssd1306_WriteChar(char ch, FontDef Font, SSD1306_COLOR color);

/**
 * @brief 写入一个字符串到屏幕。
 * @param str 字符串。
 * @param Font 字体定义。
 * @param color 字体颜色。
 * @return 操作状态（1 = 成功，0 = 失败）。
 */
char ssd1306_WriteString(const char* str, FontDef Font, SSD1306_COLOR color);

/**
 * @brief 设置屏幕光标位置。
 * @param x X 坐标。
 * @param y Y 坐标。
 */
void ssd1306_SetCursor(uint8_t x, uint8_t y);

/**
 * @brief 反转屏幕颜色。
 */
void ssd1306_InvertColors(void);

#endif  // _SSD1306_H
