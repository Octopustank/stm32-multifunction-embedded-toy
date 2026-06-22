// 防止重复包含头文件
#ifndef _FONTS_H
#define _FONTS_H

// 包含标准整数类型头文件
#include <stdint.h>

// 定义字体结构体，用于描述字体的宽度、高度和数据
// 结构体 FontDef 用于存储字体信息，包括：
// 1. 字体宽度（像素）
// 2. 字体高度（像素）
// 3. 字体数据的指针
typedef struct {
    const uint8_t FontWidth;    // 字体宽度，以像素为单位
    uint8_t FontHeight;         // 字体高度，以像素为单位
    const uint16_t *data;       // 字体数据的指针，指向存储字体像素的数组
} FontDef;

// 声明可用的三种字体，供外部程序使用
// Font_7x10: 7像素宽，10像素高的字体
// Font_11x18: 11像素宽，18像素高的字体
// Font_16x26: 16像素宽，26像素高的字体
extern FontDef Font_7x10;       // 声明 7x10 字体
extern FontDef Font_11x18;      // 声明 11x18 字体
extern FontDef Font_16x26;      // 声明 16x26 字体

// 结束头文件防重复包含的保护
#endif  // _FONTS_H