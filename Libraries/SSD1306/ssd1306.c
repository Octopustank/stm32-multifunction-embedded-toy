#include "ssd1306.h"
// Screenbuffer
static uint8_t SSD1306_Buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];

// Screen object
static SSD1306_t SSD1306;


//
//  Send a byte to the command register
//
static uint8_t ssd1306_WriteCommand(I2C_HandleTypeDef *hi2c, uint8_t command)
{
    return HAL_I2C_Mem_Write(hi2c, SSD1306_I2C_ADDR, 0x00, 1, &command, 1, 10);
}

//
//  初始化 OLED 屏幕
//
uint8_t ssd1306_Init(I2C_HandleTypeDef *hi2c)
{
    // 等待屏幕启动完成
    HAL_Delay(100);
    int status = 0;

    // 初始化显示
    status += ssd1306_WriteCommand(hi2c, 0xAE);   // 关闭显示
    status += ssd1306_WriteCommand(hi2c, 0x20);   // 设置内存寻址模式
    status += ssd1306_WriteCommand(hi2c, 0x10);   // 选择寻址模式，0x00 - 水平，0x01 - 垂直，0x10 - 页面模式（重置），0x11 - 无效
    status += ssd1306_WriteCommand(hi2c, 0xB0);   // 设置页面起始地址，0到7
    status += ssd1306_WriteCommand(hi2c, 0xC8);   // 设置 COM 输出扫描方向
    status += ssd1306_WriteCommand(hi2c, 0x00);   // 设置低列地址
    status += ssd1306_WriteCommand(hi2c, 0x10);   // 设置高列地址
    status += ssd1306_WriteCommand(hi2c, 0x40);   // 设置起始行地址
    status += ssd1306_WriteCommand(hi2c, 0x81);   // 设置对比度控制寄存器
    status += ssd1306_WriteCommand(hi2c, 0xFF);
    status += ssd1306_WriteCommand(hi2c, 0xA1);   // 设置段重映射，从0到127
    status += ssd1306_WriteCommand(hi2c, 0xA6);   // 设置正常显示模式

    status += ssd1306_WriteCommand(hi2c, 0xA8);   // 设置多路复用比率（1到64）
    status += ssd1306_WriteCommand(hi2c, SSD1306_HEIGHT - 1);

    status += ssd1306_WriteCommand(hi2c, 0xA4);   // 0xA4，输出跟随 RAM 内容；0xA5，输出忽略 RAM 内容
    status += ssd1306_WriteCommand(hi2c, 0xD3);   // 设置显示偏移
    status += ssd1306_WriteCommand(hi2c, 0x00);   // 无偏移
    status += ssd1306_WriteCommand(hi2c, 0xD5);   // 设置显示时钟分频比率/振荡器频率
    status += ssd1306_WriteCommand(hi2c, 0xF0);   // 设置分频比率
    status += ssd1306_WriteCommand(hi2c, 0xD9);   // 设置预充电周期
    status += ssd1306_WriteCommand(hi2c, 0x22);

    status += ssd1306_WriteCommand(hi2c, 0xDA);   // 设置 COM 引脚硬件配置
    status += ssd1306_WriteCommand(hi2c, SSD1306_COM_LR_REMAP << 5 | SSD1306_COM_ALTERNATIVE_PIN_CONFIG << 4 | 0x02);

    status += ssd1306_WriteCommand(hi2c, 0xDB);   // 设置 VCOMH 电压
    status += ssd1306_WriteCommand(hi2c, 0x20);   // 0x20，0.77xVcc
    status += ssd1306_WriteCommand(hi2c, 0x8D);   // 设置 DC-DC 启用
    status += ssd1306_WriteCommand(hi2c, 0x14);   //
    status += ssd1306_WriteCommand(hi2c, 0xAF);   // 打开 SSD1306 面板

    if (status != 0) {
        return 1;   // 如果初始化失败，返回 1
    }

    // 清屏
    ssd1306_Fill(Black);

    // 刷新屏幕
    ssd1306_UpdateScreen(hi2c);

    // 设置默认的屏幕参数
    SSD1306.CurrentX = 0;
    SSD1306.CurrentY = 0;

    SSD1306.Initialized = 1;

    return 0;   // 初始化成功，返回 0
}

//
//  用指定颜色填充整个屏幕
//
void ssd1306_Fill(SSD1306_COLOR color)
{
    // 用指定颜色填充屏幕缓冲区
    uint32_t i;

    for(i = 0; i < sizeof(SSD1306_Buffer); i++)
    {
        SSD1306_Buffer[i] = (color == Black) ? 0x00 : 0xFF;
    }
}

//
//  将屏幕缓冲区的内容更新到屏幕
//
void ssd1306_UpdateScreen(I2C_HandleTypeDef *hi2c)
{
    uint8_t i;

    for (i = 0; i < 8; i++) {
        // 设置页地址
        ssd1306_WriteCommand(hi2c, 0xB0 + i);
        // 设置列地址
        ssd1306_WriteCommand(hi2c, 0x00);
        ssd1306_WriteCommand(hi2c, 0x10);

        // 写入屏幕缓冲区内容
        HAL_I2C_Mem_Write(hi2c, SSD1306_I2C_ADDR, 0x40, 1, &SSD1306_Buffer[SSD1306_WIDTH * i], SSD1306_WIDTH, 100);
    }
}

//
//  在屏幕缓冲区中绘制一个像素
//  x => x 坐标
//  y => y 坐标
//  color => 像素颜色
//
void ssd1306_DrawPixel(uint8_t x, uint8_t y, SSD1306_COLOR color)
{
    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT)
    {
        // 如果坐标超出屏幕范围，则不绘制
        return;
    }

    // 判断是否需要反转像素颜色
    if (SSD1306.Inverted)
    {
        color = (SSD1306_COLOR)!color;
    }

    // 在正确的颜色下绘制像素
    if (color == White)
    {
        SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] |= 1 << (y % 8);
    }
    else
    {
        SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1 << (y % 8));
    }
}


//
//  向屏幕缓冲区写入一个字符
//  ch      => 要写入的字符
//  Font    => 字体
//  color   => 颜色（黑或白）
//
char ssd1306_WriteChar(char ch, FontDef Font, SSD1306_COLOR color)
{
    uint32_t i, b, j;

    // 检查当前行是否有足够空间
    if (SSD1306_WIDTH <= (SSD1306.CurrentX + Font.FontWidth) ||
        SSD1306_HEIGHT <= (SSD1306.CurrentY + Font.FontHeight))
    {
        // 如果没有足够的空间，则不能继续绘制
        return 0;
    }

    // 将字体转换为屏幕缓冲区数据
    for (i = 0; i < Font.FontHeight; i++)
    {
        b = Font.data[(ch - 32) * Font.FontHeight + i];
        for (j = 0; j < Font.FontWidth; j++)
        {
            if ((b << j) & 0x8000)
            {
                ssd1306_DrawPixel(SSD1306.CurrentX + j, (SSD1306.CurrentY + i), (SSD1306_COLOR) color);
            }
            else
            {
                ssd1306_DrawPixel(SSD1306.CurrentX + j, (SSD1306.CurrentY + i), (SSD1306_COLOR)!color);
            }
        }
    }

    // 更新当前光标位置
    SSD1306.CurrentX += Font.FontWidth;

    // 返回已写入的字符以验证
    return ch;
}

//
//  向屏幕缓冲区写入一个字符串
//
char ssd1306_WriteString(const char* str, FontDef Font, SSD1306_COLOR color)
{
    // 逐字符写入直到遇到空字符
    while (*str)
    {
        if (ssd1306_WriteChar(*str, Font, color) != *str)
        {
            // 如果字符无法写入，返回当前字符
            return *str;
        }

        // 写入下一个字符
        str++;
    }

    // 所有字符均已成功写入
    return *str;
}

//
//  反转背景和前景颜色
//
void ssd1306_InvertColors(void)
{
    SSD1306.Inverted = !SSD1306.Inverted;
}

//
//  设置光标位置
//
void ssd1306_SetCursor(uint8_t x, uint8_t y)
{
    SSD1306.CurrentX = x;
    SSD1306.CurrentY = y;
}
