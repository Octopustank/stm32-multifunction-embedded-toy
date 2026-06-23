#include "tim.h"

TIM_HandleTypeDef htim4;

/* SR04 capture globals (used by HCSR04.c) */
uint8_t  TIM4CH2_CAPTURE_STA  = 0;
uint16_t TIM4CH2_CAPTURE_VAL  = 0;

/*
 * TIM4: CH2 input capture on PB7 (Echo).
 * Clock: 72MHz (APB1 timer), prescaler 71 → 1MHz → 1us tick.
 * CH2 captures both edges to measure Echo high pulse width.
 */
void MX_TIM4_Init(void)
{
    TIM_IC_InitTypeDef sConfigIC = {0};

    htim4.Instance = TIM4;
    htim4.Init.Prescaler = 71;
    htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim4.Init.Period = 65535;
    htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_IC_Init(&htim4) != HAL_OK)
        Error_Handler();

    sConfigIC.ICPolarity  = TIM_INPUTCHANNELPOLARITY_RISING;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
    sConfigIC.ICFilter    = 0;
    if (HAL_TIM_IC_ConfigChannel(&htim4, &sConfigIC, TIM_CHANNEL_2) != HAL_OK)
        Error_Handler();
}

void HAL_TIM_IC_MspInit(TIM_HandleTypeDef *htim)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if (htim->Instance == TIM4)
    {
        __HAL_RCC_TIM4_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();

        /* PB7 = TIM4_CH2 (Echo): input, no pull */
        GPIO_InitStruct.Pin = GPIO_PIN_7;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        HAL_NVIC_SetPriority(TIM4_IRQn, 1, 0);
        HAL_NVIC_EnableIRQ(TIM4_IRQn);
    }
}

void HAL_TIM_IC_MspDeInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4)
    {
        __HAL_RCC_TIM4_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_7);
        HAL_NVIC_DisableIRQ(TIM4_IRQn);
    }
}

/*
 * TIM4 interrupt handler — SR04 echo capture.
 * Rising edge: reset counter, clear overflow count, switch to falling edge.
 * Falling edge: save captured value, set completion flag.
 * Counter overflow: increment overflow count.
 */
void TIM4_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim4);
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM4) return;

    if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2)
    {
        if (TIM4CH2_CAPTURE_STA & 0x40)     /* second capture (falling edge) */
        {
            TIM4CH2_CAPTURE_STA |= 0x80;    /* capture complete */
            TIM4CH2_CAPTURE_VAL  = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
            /* switch back to rising edge for next measurement */
            __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_2,
                                          TIM_INPUTCHANNELPOLARITY_RISING);
        }
        else                                 /* first capture (rising edge) */
        {
            TIM4CH2_CAPTURE_STA  = 0;        /* clear overflow count */
            TIM4CH2_CAPTURE_VAL  = 0;
            TIM4CH2_CAPTURE_STA |= 0x40;     /* mark first capture done */
            __HAL_TIM_SET_COUNTER(htim, 0);
            /* switch to falling edge */
            __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_2,
                                          TIM_INPUTCHANNELPOLARITY_FALLING);
        }
    }
}

/* TIM4 period-elapsed (overflow) callback */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM4) return;

    if (TIM4CH2_CAPTURE_STA & 0x40)         /* capture in progress */
    {
        if ((TIM4CH2_CAPTURE_STA & 0x3f) < 0x3f)
        {
            TIM4CH2_CAPTURE_STA++;
        }
        else
        {
            TIM4CH2_CAPTURE_STA |= 0x80;    /* force complete on timeout */
            TIM4CH2_CAPTURE_VAL  = 0xffff;
        }
    }
}
