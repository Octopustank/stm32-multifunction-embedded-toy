/*
 * main.c - Application entry (runs as RT-Thread main thread)
 *
 * Architecture (Blackboard Pattern):
 *   sensor_thread (prio 8)  → sensor_data (mutex) ← oled_thread (prio 12)
 *                                   ↕ semaphore
 *                            future: mqtt_thread
 *
 *   key_thread (prio 10)   → key_sem  → led_thread (prio 11)
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
#include "tim.h"
#include "HCSR04.h"

/* ---- LED Definitions ----------------------------------------------------- */

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

#define LED_ON    GPIO_PIN_RESET
#define LED_OFF   GPIO_PIN_SET

/* ---- Sensor Data (Blackboard) -------------------------------------------- */

struct SensorData {
    float    temp;
    float    humi;
    uint16_t light;
    uint16_t distance;   /* cm, SR04 */
    uint8_t  is_valid;   /* 1=OK, 0=stale, 2=hw fault */
};

static struct SensorData     sensor_data;
static struct rt_mutex       sensor_mutex;
static struct rt_mutex       i2c_mutex;
static struct rt_semaphore   sensor_sem;   /* notify OLED consumer */
static struct rt_semaphore   key_sem;      /* CENTER key → LED thread */

/* ---- Thread State -------------------------------------------------------- */

static int auto_mode = 1;
static int cur_led   = 0;

/* ---- Forward Declarations ------------------------------------------------ */

static void led_thread_entry(void *param);
static void key_thread_entry(void *param);
static void sensor_thread_entry(void *param);
static void oled_thread_entry(void *param);

/* ---- Helpers ------------------------------------------------------------- */

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
 * Non-blocking key-press detector (falling-edge with debounce).
 * Does NOT wait for release — other keys remain responsive.
 */
static rt_bool_t key_pressed(GPIO_TypeDef *port, uint16_t pin)
{
    static uint8_t prev[5];
    uint8_t idx;

    if      (port == K1_LEFT_GPIO_Port   && pin == K1_LEFT_Pin)   idx = 0;
    else if (port == K2_RIGHT_GPIO_Port  && pin == K2_RIGHT_Pin)  idx = 1;
    else if (port == K3_UP_GPIO_Port     && pin == K3_UP_Pin)     idx = 2;
    else if (port == K4_DOWN_GPIO_Port   && pin == K4_DOWN_Pin)   idx = 3;
    else if (port == K5_CENTER_GPIO_Port && pin == K5_CENTER_Pin) idx = 4;
    else return RT_FALSE;

    uint8_t cur = (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET) ? 1 : 0;
    if (cur && !prev[idx]) {
        rt_thread_mdelay(20);
        if (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET) {
            prev[idx] = 1;
            return RT_TRUE;
        }
    }
    if (!cur) prev[idx] = 0;
    return RT_FALSE;
}

/* ---- LED Animations (semaphore-driven) ----------------------------------- */

static rt_bool_t waterfall_once(void)
{
    for (uint8_t i = 0; i < LED_NUM; i++) {
        HAL_GPIO_WritePin(leds[i].port, leds[i].pin, LED_ON);
        if (rt_sem_take(&key_sem, WATERFALL_DELAY) == RT_EOK) return RT_TRUE;
        HAL_GPIO_WritePin(leds[i].port, leds[i].pin, LED_OFF);
    }
    for (uint8_t i = LED_NUM - 2; i > 0; i--) {
        HAL_GPIO_WritePin(leds[i].port, leds[i].pin, LED_ON);
        if (rt_sem_take(&key_sem, WATERFALL_DELAY) == RT_EOK) return RT_TRUE;
        HAL_GPIO_WritePin(leds[i].port, leds[i].pin, LED_OFF);
    }
    return RT_FALSE;
}

static rt_bool_t blink_all(uint8_t times)
{
    for (uint8_t t = 0; t < times; t++) {
        for (uint8_t i = 0; i < LED_NUM; i++)
            HAL_GPIO_WritePin(leds[i].port, leds[i].pin, LED_ON);
        if (rt_sem_take(&key_sem, BLINK_DELAY) == RT_EOK) return RT_TRUE;
        for (uint8_t i = 0; i < LED_NUM; i++)
            HAL_GPIO_WritePin(leds[i].port, leds[i].pin, LED_OFF);
        if (t < times - 1 && rt_sem_take(&key_sem, BLINK_DELAY) == RT_EOK)
            return RT_TRUE;
    }
    if (rt_sem_take(&key_sem, 400) == RT_EOK) return RT_TRUE;
    return RT_FALSE;
}

/* ---- LED Thread (prio 11) ------------------------------------------------ */

static void led_thread_entry(void *param)
{
    while (1) {
        if (auto_mode) {
            rt_bool_t stopped = RT_FALSE;
            for (uint8_t cycle = 0; cycle < CYCLE_COUNT && !stopped; cycle++)
                stopped = waterfall_once();
            if (!stopped) blink_all(2);
            if (stopped) {
                auto_mode = 0;
                cur_led = 0;
                all_leds_off();
            }
        } else {
            rt_thread_mdelay(50);
        }
    }
}

/* ---- Key Thread (prio 10) ------------------------------------------------ */

static void key_thread_entry(void *param)
{
    while (1) {
        if (key_pressed(K5_CENTER_GPIO_Port, K5_CENTER_Pin)) {
            if (auto_mode) {
                rt_sem_release(&key_sem);
            } else {
                auto_mode = 1;
                cur_led = 0;
                all_leds_off();
            }
        }
        if (!auto_mode) {
            int8_t key = 0;
            if (key_pressed(K1_LEFT_GPIO_Port,  K1_LEFT_Pin))  key = 1;
            if (key_pressed(K2_RIGHT_GPIO_Port, K2_RIGHT_Pin)) key = 2;
            if (key_pressed(K3_UP_GPIO_Port,    K3_UP_Pin))    key = 3;
            if (key_pressed(K4_DOWN_GPIO_Port,  K4_DOWN_Pin))  key = 4;

            if (key == 1 || key == 3)
                cur_led = (cur_led == 0) ? (int)(LED_NUM - 1) : cur_led - 1;
            if (key == 2 || key == 4)
                cur_led = (cur_led + 1) % LED_NUM;
            if (key != 0) set_led((uint8_t)cur_led);
        }
        rt_thread_mdelay(10);
    }
}

/* ---- Sensor Thread (producer, prio 8) ------------------------------------ */

static void sensor_thread_entry(void *param)
{
    float temp, humi;
    static float last_temp, last_humi;
    static uint8_t fail_count;

    rt_thread_mdelay(2000);   /* DHT11 warm-up */

    while (1) {
        uint8_t result = DHT11_READ_DATA(&temp, &humi);
        uint16_t light = adc_read();
        HCSR_04();
        float dist = getSR04Distance();
        uint16_t distance = (dist > 0 && dist < 4000) ? (uint16_t)(dist + 0.5f) : 0;

        rt_mutex_take(&sensor_mutex, RT_WAITING_FOREVER);
        sensor_data.light    = light;
        sensor_data.distance = distance;

        if (result == 1) {
            last_temp  = temp;
            last_humi  = humi;
            fail_count = 0;
            sensor_data.temp     = temp;
            sensor_data.humi     = humi;
            sensor_data.is_valid = 1;
        } else {
            fail_count++;
            sensor_data.temp     = last_temp;
            sensor_data.humi     = last_humi;
            sensor_data.is_valid = (fail_count > 10) ? 2 : 0;
        }
        rt_mutex_release(&sensor_mutex);

        rt_sem_release(&sensor_sem);
        rt_thread_mdelay(800);
    }
}

/* ---- OLED Thread (consumer, prio 12) ------------------------------------- */

static void oled_thread_entry(void *param)
{
    char line1[32], line2[32], line3[32];
    rt_thread_mdelay(2000);

    while (1) {
        rt_sem_take(&sensor_sem, RT_WAITING_FOREVER);

        rt_mutex_take(&sensor_mutex, RT_WAITING_FOREVER);
        struct SensorData local = sensor_data;
        rt_mutex_release(&sensor_mutex);

        if (local.is_valid == 1) {
            int t_i = (int)local.temp, t_d = (int)((local.temp - t_i) * 10 + 0.5f);
            int h_i = (int)local.humi, h_d = (int)((local.humi - h_i) * 10 + 0.5f);
            snprintf(line1, sizeof(line1), "T:%d.%dC H:%d.%d%%", t_i, t_d, h_i, h_d);
        } else if (local.is_valid == 0) {
            snprintf(line1, sizeof(line1), "reading...");
        } else {
            snprintf(line1, sizeof(line1), "DHT11 fault!");
        }
        snprintf(line2, sizeof(line2), "Light: %u", local.light);
        snprintf(line3, sizeof(line3), "Dist: %u cm", local.distance);

        rt_mutex_take(&i2c_mutex, RT_WAITING_FOREVER);
        ssd1306_Fill(Black);
        ssd1306_SetCursor(0, 0);
        ssd1306_WriteString(line1, Font_7x10, White);
        ssd1306_SetCursor(0, 16);
        ssd1306_WriteString(line2, Font_7x10, White);
        ssd1306_SetCursor(0, 32);
        ssd1306_WriteString(line3, Font_7x10, White);
        ssd1306_UpdateScreen(&hi2c1);
        rt_mutex_release(&i2c_mutex);
    }
}

/* ---- Application Entry --------------------------------------------------- */

int main(void)
{
    MX_GPIO_Init();
    MX_I2C1_Init();
    ssd1306_Init(&hi2c1);
    MX_ADC1_Init();

    /* SR04 Trig pin (PB6): output push-pull */
    {
        GPIO_InitTypeDef s = {0};
        s.Pin   = TRIG_Pin;
        s.Mode  = GPIO_MODE_OUTPUT_PP;
        s.Pull  = GPIO_NOPULL;
        s.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(TRIG_GPIO_Port, &s);
        HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_RESET);
    }

    MX_TIM4_Init();
    HAL_TIM_IC_Start_IT(&htim4, TIM_CHANNEL_2);
    __HAL_TIM_ENABLE_IT(&htim4, TIM_IT_UPDATE);  /* overflow interrupt */

    /* kernel objects (static init — no heap frag) */
    rt_mutex_init(&sensor_mutex, "s_mtx", RT_IPC_FLAG_FIFO);
    rt_mutex_init(&i2c_mutex,    "i_mtx", RT_IPC_FLAG_FIFO);
    rt_sem_init(&sensor_sem, "s_sem", 0, RT_IPC_FLAG_FIFO);
    rt_sem_init(&key_sem,    "k_sem", 0, RT_IPC_FLAG_FIFO);

    /* splash screen */
    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString("Init...", Font_7x10, White);
    ssd1306_UpdateScreen(&hi2c1);

    rt_thread_t tid;
    tid = rt_thread_create("sens", sensor_thread_entry, RT_NULL, 768,  8, 10);
    if (tid) rt_thread_startup(tid);
    tid = rt_thread_create("oled", oled_thread_entry,   RT_NULL, 1024, 12, 10);
    if (tid) rt_thread_startup(tid);
    tid = rt_thread_create("led",  led_thread_entry,    RT_NULL, 512,  11, 10);
    if (tid) rt_thread_startup(tid);
    tid = rt_thread_create("keys", key_thread_entry,    RT_NULL, 384,  10, 10);
    if (tid) rt_thread_startup(tid);

    while (1) rt_thread_mdelay(1000);
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
