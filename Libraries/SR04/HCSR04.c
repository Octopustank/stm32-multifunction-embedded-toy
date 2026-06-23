/**
 * @file HCSR04.c
 * @author Zhong Zepeng (1935595312@qq.com)
 * @brief
 * @version 0.1
 * @date 2022-11-25
 *
 * @copyright Copyright (c) 2022
 *
 */
#include "HCSR04.h"
#include "stdio.h"
#include "gpio.h"
#include "tim.h"
//#include "usart.h"
//#include "User_Debug.h"
#include <string.h>  // 添加这一行
/**
 * @brief 激活超声波定时器
 *
 */

// 全局变量定义，放在文件顶部（主函数之外）

void HCSR_04()
{
    uint32_t i;
    HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_SET);
    for (i = 0; i < 72 * 20; i++)
        __NOP();
    HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_RESET);
}

/**
 * @brief 计算超声波检测的距离
 *
 * @return float
 */

float getSR04Distance()
{
    float len = 0;
    uint32_t time = 0;
    if (TIM4CH2_CAPTURE_STA & 0X80) // 输入捕获触发
    {
        time = TIM4CH2_CAPTURE_STA & 0X3f; // 获得溢出次数
        time *= 65536;                     // 一次溢出为65536 得到溢出的时间
        time += TIM4CH2_CAPTURE_VAL;       // 溢出的时间+现在定时器的值 得到总的时间

        len = time * 342.62 * 100 / 2000000; // 计算得到距离 cm
        // 输出距离和单位
        TIM4CH2_CAPTURE_STA = 0; // 清除溢出
    }
    return len;
}

