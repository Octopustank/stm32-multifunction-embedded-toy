#include "HCSR04.h"
#include "tim.h"

void HCSR_04(void)
{
    uint32_t i;
    HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_SET);
    for (i = 0; i < 72 * 20; i++)
        __NOP();
    HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_RESET);
}

/*
 * Wait for Echo capture to complete, then calculate distance.
 * SR04 round-trip at max 4m ≈ 23ms; timeout 50ms is safe.
 * Uses HAL_Delay(1) to yield CPU via SysTick → RT-Thread scheduler.
 */
float getSR04Distance(void)
{
    int retry = 50;
    while (!(TIM4CH2_CAPTURE_STA & 0x80) && retry > 0) {
        HAL_Delay(1);
        retry--;
    }

    if (!(TIM4CH2_CAPTURE_STA & 0x80))
        return 0.0f;

    uint32_t time = (TIM4CH2_CAPTURE_STA & 0x3f) * 65536UL;
    time += TIM4CH2_CAPTURE_VAL;

    float len = time * 342.62f * 100.0f / 2000000.0f;
    TIM4CH2_CAPTURE_STA = 0;
    return len;
}
