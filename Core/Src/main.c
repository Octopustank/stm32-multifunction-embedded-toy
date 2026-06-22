/*
 * main.c - Application entry (runs as RT-Thread main thread)
 *
 * Startup flow:
 *   startup.s → entry() → rtthread_startup() → main()
 */
#include <rtthread.h>
#include <stdio.h>
#include "main.h"
#include "gpio.h"
#include "i2c.h"
#include "ssd1306.h"
#include "dht11.h"
#include "adc.h"

/* ---- LED & Key Definitions ---------------------------------------------- */

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} LED_t;

static const LED_t leds[] = {
    {GPIOB, LED1_Pin},
    {GPIOB, LED2_Pin},
    {GPIOB, LED3_Pin},
    {LED4_GPIO_Port, LED4_Pin},
};
#define LED_NUM    (sizeof(leds) / sizeof(leds[0]))

#define WATERFALL_DELAY    300
#define BLINK_DELAY        250
#define CYCLE_COUNT          2
#define POLL_TICK           10

#define LED_ON    GPIO_PIN_RESET
#define LED_OFF   GPIO_PIN_SET

/* ---- Shared State ------------------------------------------------------- */

static volatile rt_bool_t auto_mode      = RT_TRUE;
static volatile int8_t    cur_led        = 0;
static volatile rt_bool_t center_pressed = RT_FALSE;

/* ---- Forward Declarations ----------------------------------------------- */

static void led_thread_entry(void *param);
static void key_thread_entry(void *param);
static void oled_thread_entry(void *param);

/* ---- Helpers ------------------------------------------------------------ */

static void all_leds_off(void)
{
    for (uint8_t i = 0; i < LED_NUM; i++)
        HAL_GPIO_WritePin(leds[i].port, leds[i].pin, LED_OFF);
}

static void set_led(uint8_t idx)
{
    all_leds_off();
    HAL_GPIO_WritePin(leds[idx].port, leds[idx].pin, LED_ON);
}

/*
 * key_pressed: blocking key detection with debounce.
 * Only called from key_thread (priority 11).
 * Returns RT_TRUE after a confirmed press-and-release.
 */
static rt_bool_t key_pressed(GPIO_TypeDef *port, uint16_t pin)
{
    if (HAL_GPIO_ReadPin(port, pin) != GPIO_PIN_RESET)
        return RT_FALSE;
    rt_thread_mdelay(20);
    if (HAL_GPIO_ReadPin(port, pin) != GPIO_PIN_RESET)
        return RT_FALSE;
    while (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET)
        rt_thread_mdelay(5);
    return RT_TRUE;
}

/*
 * delay_yield: non-blocking delay that yields to other threads.
 * Returns RT_TRUE if center_pressed flag was set by key thread.
 */
static rt_bool_t delay_yield(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += POLL_TICK) {
        uint32_t step = (ms - t < POLL_TICK) ? (ms - t) : POLL_TICK;
        rt_thread_mdelay(step);
        if (center_pressed) {
            center_pressed = RT_FALSE;
            return RT_TRUE;
        }
    }
    return RT_FALSE;
}

/*
 * LED animation helpers — use delay_yield instead of direct pin polling.
 * Only the key thread reads hardware pins.
 */
static rt_bool_t waterfall_once(void)
{
    for (uint8_t i = 0; i < LED_NUM; i++) {
        HAL_GPIO_WritePin(leds[i].port, leds[i].pin, LED_ON);
        if (delay_yield(WATERFALL_DELAY)) return RT_TRUE;
        HAL_GPIO_WritePin(leds[i].port, leds[i].pin, LED_OFF);
    }
    for (uint8_t i = LED_NUM - 2; i > 0; i--) {
        HAL_GPIO_WritePin(leds[i].port, leds[i].pin, LED_ON);
        if (delay_yield(WATERFALL_DELAY)) return RT_TRUE;
        HAL_GPIO_WritePin(leds[i].port, leds[i].pin, LED_OFF);
    }
    return RT_FALSE;
}

static rt_bool_t blink_all(uint8_t times)
{
    for (uint8_t t = 0; t < times; t++) {
        for (uint8_t i = 0; i < LED_NUM; i++)
            HAL_GPIO_WritePin(leds[i].port, leds[i].pin, LED_ON);
        if (delay_yield(BLINK_DELAY)) return RT_TRUE;
        for (uint8_t i = 0; i < LED_NUM; i++)
            HAL_GPIO_WritePin(leds[i].port, leds[i].pin, LED_OFF);
        if (t < times - 1) {
            if (delay_yield(BLINK_DELAY)) return RT_TRUE;
        }
    }
    if (delay_yield(400)) return RT_TRUE;
    return RT_FALSE;
}

/* ---- OLED + DHT11 Display Thread ---------------------------------------- */

static void oled_thread_entry(void *param)
{
    float temp = 0, humi = 0;
    char line1[32], line2[32];

    rt_thread_mdelay(2000);
    DHT11_READ_DATA(&temp, &humi);  /* discard first reading after power-up */

    while (1) {
        uint16_t light = adc_read();
        uint8_t result = DHT11_READ_DATA(&temp, &humi);
        if (result == 1) {
            int t_i = (int)temp, t_d = (int)((temp - t_i) * 10 + 0.5f);
            int h_i = (int)humi, h_d = (int)((humi - h_i) * 10 + 0.5f);

            snprintf(line1, sizeof(line1), "T:%d.%dC H:%d.%d%%", t_i, t_d, h_i, h_d);
        } else {
            snprintf(line1, sizeof(line1), "err:%d no reply", result);
        }
        snprintf(line2, sizeof(line2), "Light: %u", light);

        ssd1306_Fill(Black);
        ssd1306_SetCursor(0, 0);
        ssd1306_WriteString(line1, Font_7x10, White);
        ssd1306_SetCursor(0, 16);
        ssd1306_WriteString(line2, Font_7x10, White);
        ssd1306_UpdateScreen(&hi2c1);

        rt_thread_mdelay(2500);
    }
}

/* ---- LED Animation Thread ----------------------------------------------- */

static void led_thread_entry(void *param)
{
    while (1) {
        if (auto_mode) {
            rt_bool_t stopped = RT_FALSE;
            for (uint8_t cycle = 0; cycle < CYCLE_COUNT && !stopped; cycle++)
                stopped = waterfall_once();
            if (!stopped)
                blink_all(2);
            if (stopped) {
                auto_mode = RT_FALSE;
                cur_led = 0;
                all_leds_off();
            }
        } else {
            rt_thread_mdelay(50);
        }
    }
}

/* ---- Key Scan Thread ----------------------------------------------------

 * This thread is the SINGLE owner of hardware pin reading.
 * LED thread only reads the center_pressed flag.
 */
static void key_thread_entry(void *param)
{
    while (1) {
        if (key_pressed(K5_CENTER_GPIO_Port, K5_CENTER_Pin)) {
            if (auto_mode) {
                /*
                 * Auto mode: signal LED thread via flag.
                 * LED thread will stop the animation and flip auto_mode to FALSE.
                 */
                center_pressed = RT_TRUE;
            } else {
                /*
                 * Manual mode: toggle to auto directly.
                 * LED thread will see auto_mode == TRUE on its next loop.
                 */
                auto_mode = RT_TRUE;
                cur_led = 0;
                all_leds_off();
            }
        }

        if (!auto_mode) {
            int8_t key = 0;

            if (key_pressed(K1_LEFT_GPIO_Port, K1_LEFT_Pin))   key = 1;
            if (key_pressed(K2_RIGHT_GPIO_Port, K2_RIGHT_Pin)) key = 2;
            if (key_pressed(K3_UP_GPIO_Port, K3_UP_Pin))       key = 3;
            if (key_pressed(K4_DOWN_GPIO_Port, K4_DOWN_Pin))   key = 4;

            if (key == 1 || key == 3)
                cur_led = (cur_led == 0) ? (int8_t)(LED_NUM - 1) : cur_led - 1;
            if (key == 2 || key == 4)
                cur_led = (cur_led + 1) % LED_NUM;
            if (key != 0)
                set_led((uint8_t)cur_led);
        }

        rt_thread_mdelay(10);
    }
}

/* ---- Application Entry (RTT main thread) -------------------------------- */

int main(void)
{
    MX_GPIO_Init();

    MX_I2C1_Init();
    ssd1306_Init(&hi2c1);
    MX_ADC1_Init();

    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString("DHT11 init...", Font_11x18, White);
    ssd1306_UpdateScreen(&hi2c1);

    rt_thread_t tid = rt_thread_create("oled",
                                        oled_thread_entry,
                                        RT_NULL,
                                        768, 12, 10);
    if (tid != RT_NULL)
        rt_thread_startup(tid);

    tid = rt_thread_create("led",
                                        led_thread_entry,
                                        RT_NULL,
                                        512, 10, 10);
    if (tid != RT_NULL)
        rt_thread_startup(tid);

    tid = rt_thread_create("keys",
                           key_thread_entry,
                           RT_NULL,
                           384, 11, 10);
    if (tid != RT_NULL)
        rt_thread_startup(tid);

    while (1) {
        rt_thread_mdelay(1000);
    }

    return RT_EOK;
}

/* ---- System Clock Configuration (called from board.c) ------------------- */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
        Error_Handler();

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                  |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
        Error_Handler();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}
