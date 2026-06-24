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
#include <string.h>
#include "main.h"
#include "gpio.h"
#include "i2c.h"
#include "ssd1306.h"
#include "dht11.h"
#include "adc.h"
#include "tim.h"
#include "HCSR04.h"
#include "usart.h"
#include "esp8266_mqtt.h"
#include "max7219.h"
#include "snake.h"
#include "net_auto_config.h"

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

/* ---- Game State ---------------------------------------------------------- */

static int         game_mode;   /* 0=idle, 1=playing, 2=dead */
static snake_dir_t g_dir;       /* input from key thread */
static int         g_serial_snake; /* serial input mode */

/* ---- MQTT State ---------------------------------------------------------- */

static char mqtt_ssid[32], mqtt_pwd[32];
static char mqtt_broker[32], mqtt_port[8], mqtt_client[32];
static char mqtt_topic[48];
static int  mqtt_connected;
static int  mqtt_sub_active;        /* 1 if subscribed to mqtt_topic */
static volatile int esp_cmd, esp_result;
static struct rt_semaphore esp_go_sem;
static struct rt_semaphore esp_done_sem;

static int        autopub_enabled = AUTO_PUB_ENABLE;
#if AUTO_SUB_ECHO
static int        sub_echo = AUTO_SUB_ECHO;
#endif
static rt_tick_t  last_autopub;

/* ---- Forward Declarations ------------------------------------------------ */

static void led_thread_entry(void *param);
static void key_thread_entry(void *param);
static void sensor_thread_entry(void *param);
static void oled_thread_entry(void *param);
static void shell_thread_entry(void *param);
static void esp_thread_entry(void *param);
static void game_thread_entry(void *param);

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
        /* ---- game-mode keys ---- */
        if (game_mode == 1) {
            if (key_pressed(K1_LEFT_GPIO_Port,  K1_LEFT_Pin))  g_dir = DIR_LEFT;
            if (key_pressed(K2_RIGHT_GPIO_Port, K2_RIGHT_Pin)) g_dir = DIR_RIGHT;
            if (key_pressed(K3_UP_GPIO_Port,    K3_UP_Pin))    g_dir = DIR_UP;
            if (key_pressed(K4_DOWN_GPIO_Port,  K4_DOWN_Pin))  g_dir = DIR_DOWN;
            if (key_pressed(K5_CENTER_GPIO_Port, K5_CENTER_Pin)) game_mode = 0;
            rt_thread_mdelay(10);
            continue;
        }
        if (game_mode == 2) {
            if (key_pressed(K3_UP_GPIO_Port, K3_UP_Pin))
                { snake_init(); game_mode = 1; }
            if (key_pressed(K5_CENTER_GPIO_Port, K5_CENTER_Pin))
                game_mode = 0;
            rt_thread_mdelay(10);
            continue;
        }

        /* ---- normal-mode keys ---- */
            if (key_pressed(K3_UP_GPIO_Port, K3_UP_Pin))
                { game_mode = 1; snake_init(); g_serial_snake = 1; continue; }
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
    char line1[32], line2[24], line3[16], line4[24];
    rt_thread_mdelay(2000);

    while (1) {
        rt_sem_take(&sensor_sem, RT_WAITING_FOREVER);

        rt_mutex_take(&sensor_mutex, RT_WAITING_FOREVER);
        struct SensorData local = sensor_data;
        rt_mutex_release(&sensor_mutex);

        if (local.is_valid == 1) {
            int t_i = (int)local.temp, t_d = (int)((local.temp - t_i) * 10 + 0.5f);
            int h_i = (int)local.humi, h_d = (int)((local.humi - h_i) * 10 + 0.5f);
            if (t_d >= 10) { t_i++; t_d -= 10; }
            if (h_d >= 10) { h_i++; h_d -= 10; }
            snprintf(line1, sizeof(line1), "T:%d.%dC H:%d.%d%%", t_i, t_d, h_i, h_d);
        } else if (local.is_valid == 0) {
            snprintf(line1, sizeof(line1), "reading...");
        } else {
            snprintf(line1, sizeof(line1), "DHT11 fault!");
        }
        snprintf(line2, sizeof(line2), "L:%u D:%ucm", local.light, local.distance);

        snprintf(line3, sizeof(line3), "W:%s M:%s",
            mqtt_connected >= 1 ? "OK" : (mqtt_connected < 0 ? "ER" : "--"),
            mqtt_connected == 2 ? "OK" : (mqtt_connected < -1 ? "ER" : "--"));

        if (mqtt_sub_active && mqtt_topic[0])
            snprintf(line4, sizeof(line4), "Sub: %.13s", mqtt_topic);
        else
            snprintf(line4, sizeof(line4), "Sub: --");

        rt_mutex_take(&i2c_mutex, RT_WAITING_FOREVER);
        ssd1306_Fill(Black);
        ssd1306_SetCursor(0, 0);
        ssd1306_WriteString(line1, Font_7x10, White);
        ssd1306_SetCursor(0, 16);
        ssd1306_WriteString(line2, Font_7x10, White);
        ssd1306_SetCursor(0, 32);
        ssd1306_WriteString(line3, Font_7x10, White);
        ssd1306_SetCursor(0, 48);
        ssd1306_WriteString(line4, Font_7x10, White);
        ssd1306_UpdateScreen(&hi2c1);
        rt_mutex_release(&i2c_mutex);
    }
}

/* ---- Shell Thread (UART command parser with history) -------------------- */

#define HIST_SIZE  4
#define HIST_LEN   24
#define CMD_BUF    24

static void shell_thread_entry(void *param)
{
    static char cmd[CMD_BUF], history[HIST_SIZE][HIST_LEN];
    static int  hist_wr, hist_cnt, hist_cur;
    static uint8_t pos, esc_state;
    char buf[64];

    uart_puts("\r\n \\ | /\r\n");
    uart_puts("- RT -     Thread Operating System\r\n");
    snprintf(buf, sizeof(buf), " / | \\     %d.%d.%d build %s %s\r\n",
             RT_VERSION, RT_SUBVERSION, RT_REVISION, __DATE__, __TIME__);
    uart_puts(buf);
    uart_puts(" 2006 - 2022 Copyright by RT-Thread team\r\n\r\n");
    uart_puts("rtt> ");

    while (1) {
        /* yield UART to game thread during serial snake */
        if (game_mode == 1 && g_serial_snake) {
            rt_thread_mdelay(50);
            continue;
        }

        int c = uart_getc();
        if (c < 0) { rt_thread_mdelay(10); continue; }

        /* ---- ESC sequence parser (arrow keys) ---- */
        if (esc_state == 0 && c == 0x1B)      { esc_state = 1; continue; }
        if (esc_state == 1) {
            esc_state = (c == '[') ? 2 : 0;
            continue;
        }
        if (esc_state == 2) {
            esc_state = 0;

            if (c == 'A') {     /* UP: browse older history */
                if (hist_cnt == 0) continue;
                if (hist_cur < hist_cnt) {
                    /* save current input if at bottom */
                    if (hist_cur == 0) memcpy(history[hist_wr], cmd, pos + 1);

                    hist_cur++;
                    int idx = (hist_wr - hist_cur + HIST_SIZE) % HIST_SIZE;
                    /* clear line, show recalled command */
                    while (pos--) uart_puts("\b \b");
                    uart_puts(history[idx]);
                    strcpy(cmd, history[idx]);
                    pos = strlen(cmd);
                }
                continue;
            }
            if (c == 'B') {     /* DOWN: browse newer history */
                if (hist_cur == 0) continue;
                hist_cur--;
                /* clear line */
                while (pos--) uart_puts("\b \b");
                if (hist_cur == 0) {
                    cmd[0] = '\0'; pos = 0; /* bottom: restore current input */
                    memcpy(cmd, history[hist_wr], HIST_LEN);
                    cmd[HIST_LEN-1] = '\0';
                    pos = strlen(cmd);
                    uart_puts(cmd);
                } else {
                    int idx = (hist_wr - hist_cur + HIST_SIZE) % HIST_SIZE;
                    uart_puts(history[idx]);
                    strcpy(cmd, history[idx]);
                    pos = strlen(cmd);
                }
                continue;
            }
            /* other CSI keys: ignore */
            continue;
        }

        /* ---- Backspace ---- */
        if (c == '\b' || c == 127) {
            if (pos > 0) { uart_puts("\b \b"); pos--; cmd[pos] = '\0'; }
            continue;
        }

        /* ---- Echo printable ---- */
        uart_putc((char)c);

        /* ---- Enter: execute ---- */
        if (c == '\r' || c == '\n') {
            cmd[pos] = '\0';
            uart_puts("\r\n");

            if (pos == 0) goto prompt;

            /* save to history (skip duplicates) */
            if (hist_cnt == 0 || strcmp(cmd, history[(hist_wr - 1 + HIST_SIZE) % HIST_SIZE])) {
                strncpy(history[hist_wr], cmd, HIST_LEN - 1);
                history[hist_wr][HIST_LEN - 1] = '\0';
                hist_wr = (hist_wr + 1) % HIST_SIZE;
                if (hist_cnt < HIST_SIZE) hist_cnt++;
            }
            hist_cur = 0;

            /* ---- command dispatch ---- */
            if      (!strcmp(cmd, "help")) uart_puts("help temp humi light dist stat led on|off snake autopub sublist subecho\r\nwifi SSID PWD   mqtt IP PORT ID   connect   mqttconn\r\npub   sub TOPIC   unsub TOPIC\r\n");
            else if (!strcmp(cmd, "stat")) {
                rt_mutex_take(&sensor_mutex, RT_WAITING_FOREVER);
                struct SensorData local = sensor_data;
                rt_mutex_release(&sensor_mutex);
                char buf[96];
                int ti = (int)local.temp, td = (int)((local.temp - ti) * 10 + 0.5f);
                int hi = (int)local.humi, hd = (int)((local.humi - hi) * 10 + 0.5f);
                snprintf(buf, sizeof(buf), "T:%d.%dC H:%d.%d%% Light:%u Dist:%ucm ok:%d esp:%d sub:%d apub:%d\r\n",
                         ti, td, hi, hd, local.light, local.distance, local.is_valid, mqtt_connected,
                         mqtt_sub_active, autopub_enabled);
                uart_puts(buf);
            }
            else if (!strcmp(cmd, "temp")) {
                rt_mutex_take(&sensor_mutex, RT_WAITING_FOREVER);
                float t = sensor_data.temp;
                rt_mutex_release(&sensor_mutex);
                char buf[16];
                int ti = (int)t, td = (int)((t - ti) * 10 + 0.5f);
                snprintf(buf, sizeof(buf), "%d.%d C\r\n", ti, td); uart_puts(buf);
            }
            else if (!strcmp(cmd, "humi")) {
                rt_mutex_take(&sensor_mutex, RT_WAITING_FOREVER);
                float h = sensor_data.humi;
                rt_mutex_release(&sensor_mutex);
                char buf[16];
                int hi = (int)h, hd = (int)((h - hi) * 10 + 0.5f);
                snprintf(buf, sizeof(buf), "%d.%d %%\r\n", hi, hd); uart_puts(buf);
            }
            else if (!strcmp(cmd, "light")) {
                rt_mutex_take(&sensor_mutex, RT_WAITING_FOREVER);
                uint16_t l = sensor_data.light;
                rt_mutex_release(&sensor_mutex);
                char buf[16]; snprintf(buf, sizeof(buf), "%u\r\n", l); uart_puts(buf);
            }
            else if (!strcmp(cmd, "dist")) {
                rt_mutex_take(&sensor_mutex, RT_WAITING_FOREVER);
                uint16_t d = sensor_data.distance;
                rt_mutex_release(&sensor_mutex);
                char buf[16]; snprintf(buf, sizeof(buf), "%u cm\r\n", d); uart_puts(buf);
            }
            else if (!strcmp(cmd, "led on"))  { auto_mode = 1; cur_led = 0; all_leds_off(); uart_puts("ok\r\n"); }
            else if (!strcmp(cmd, "led off")) { auto_mode = 0; cur_led = 0; all_leds_off(); uart_puts("ok\r\n"); }
            else if (!strcmp(cmd, "autopub")) {
                autopub_enabled = !autopub_enabled;
                uart_puts(autopub_enabled ? "autopub ON\r\n" : "autopub OFF\r\n");
            }
#if AUTO_SUB_ECHO
            else if (!strcmp(cmd, "subecho")) {
                sub_echo = !sub_echo;
                uart_puts(sub_echo ? "subecho ON\r\n" : "subecho OFF\r\n");
            }
#endif
            else if (!strcmp(cmd, "snake"))  { snake_init(); game_mode = 1; g_serial_snake = 1; uart_puts("WASD=move Enter=end Ctrl+C=abort\r\n"); }
            else if (!strncmp(cmd, "wifi ", 5)) {
                char *sp = strchr(cmd + 5, ' ');
                if (!sp) { uart_puts("usage: wifi SSID PWD\r\n"); goto prompt; }
                *sp = '\0';
                strncpy(mqtt_ssid, cmd + 5, sizeof(mqtt_ssid) - 1);
                strncpy(mqtt_pwd, sp + 1, sizeof(mqtt_pwd) - 1);
                uart_puts("saved\r\n");
            }
            else if (!strcmp(cmd, "connect")) {
                uart_puts("Connecting WiFi...\r\n");
                esp_cmd = 1;
                rt_sem_release(&esp_go_sem);
                if (rt_sem_take(&esp_done_sem, 25000) == RT_EOK) {
                    if (esp_result == 0) uart_puts("WiFi connected!\r\n");
                    else { char b[48]; snprintf(b, sizeof(b), "WiFi fail %d\r\n", (int)esp_result); uart_puts(b); }
                } else uart_puts("WiFi timeout\r\n");
            }
            else if (!strncmp(cmd, "mqtt ", 5)) {
                char *p1 = strchr(cmd + 5, ' ');
                char *p2 = p1 ? strchr(p1 + 1, ' ') : NULL;
                if (!p1 || !p2) { uart_puts("usage: mqtt IP PORT ID\r\n"); goto prompt; }
                *p1 = '\0'; *p2 = '\0';
                strncpy(mqtt_broker, cmd + 5, sizeof(mqtt_broker) - 1);
                strncpy(mqtt_port,   p1 + 1, sizeof(mqtt_port) - 1);
                strncpy(mqtt_client, p2 + 1, sizeof(mqtt_client) - 1);
                uart_puts("saved\r\n");
            }
            else if (!strcmp(cmd, "mqttconn")) {
                uart_puts("Connecting MQTT...\r\n");
                esp_cmd = 2;
                rt_sem_release(&esp_go_sem);
                if (rt_sem_take(&esp_done_sem, 15000) == RT_EOK) {
                    if (esp_result == 0) uart_puts("MQTT connected!\r\n");
                    else { char b[48]; snprintf(b, sizeof(b), "MQTT fail %d\r\n", (int)esp_result); uart_puts(b); }
                } else uart_puts("MQTT timeout\r\n");
            }
            else if (!strcmp(cmd, "pub")) {
                esp_cmd = 3;
                rt_sem_release(&esp_go_sem);
                rt_sem_take(&esp_done_sem, 10000);
                uart_puts(esp_result == 0 ? "pub ok\r\n" : "pub fail\r\n");
            }
            else if (!strncmp(cmd, "test ", 5)) {
                esp_cmd = 7;
                strncpy(mqtt_topic, cmd + 5, sizeof(mqtt_topic) - 1);
                rt_sem_release(&esp_go_sem);
                rt_sem_take(&esp_done_sem, 10000);
                uart_puts(esp_result == 0 ? "test ok\r\n" : "test fail\r\n");
            }
            else if (!strncmp(cmd, "sub ", 4)) {
                strncpy(mqtt_topic, cmd + 4, sizeof(mqtt_topic) - 1);
                esp_cmd = 4;
                rt_sem_release(&esp_go_sem);
                if (rt_sem_take(&esp_done_sem, 10000) == RT_EOK) {
                    if (esp_result == 0) {
                        uart_puts("sub ok\r\n");
                        mqtt_sub_active = 1;
                    }
                    else { char b[32]; snprintf(b, sizeof(b), "sub fail %d\r\n", (int)esp_result); uart_puts(b); }
                } else uart_puts("sub timeout\r\n");
            }
            else if (!strncmp(cmd, "unsub ", 6)) {
                strncpy(mqtt_topic, cmd + 6, sizeof(mqtt_topic) - 1);
                esp_cmd = 5;
                rt_sem_release(&esp_go_sem);
                if (rt_sem_take(&esp_done_sem, 10000) == RT_EOK) {
                    if (esp_result == 0) {
                        uart_puts("unsub ok\r\n");
                        mqtt_sub_active = 0;
                    }
                    else { char b[32]; snprintf(b, sizeof(b), "unsub fail %d\r\n", (int)esp_result); uart_puts(b); }
                } else uart_puts("unsub timeout\r\n");
            }
            else if (!strcmp(cmd, "sublist")) {
                if (!mqtt_sub_active || !mqtt_topic[0])
                    uart_puts("(no subscriptions)\r\n");
                else {
                    uart_puts("Sub: ");
                    uart_puts(mqtt_topic);
                    uart_puts("\r\n");
                }
            }
            else { uart_puts("? try: help\r\n"); }

prompt:
            pos = 0;
            uart_puts("rtt> ");
        } else if (pos < CMD_BUF - 1) {
            cmd[pos++] = (char)c;
            cmd[pos] = '\0';
        }
    }
}

/* ---- ESP Thread (async WiFi/MQTT worker) --------------------------------- */

static void esp_thread_entry(void *param)
{
    while (1) {
        rt_err_t ret = rt_sem_take(&esp_go_sem, mqtt_connected == 2 ? 800 : RT_WAITING_FOREVER);
        if (ret != RT_EOK && mqtt_connected == 2) esp_cmd = 6;

        int cmd = esp_cmd;

        if (cmd == 1) {
            esp_result = ESP8266_Init();
            if (esp_result >= 0) {
                rt_thread_mdelay(2000);
                esp_result = ESP8266_JoinAccessPoint(mqtt_ssid, mqtt_pwd);
            }
            mqtt_connected = (esp_result == 0) ? 1 : -1;
        }
        else if (cmd == 2) {
            esp_result = ESP8266_ConnectToServer(mqtt_broker, mqtt_port);
            if (esp_result >= 0)
                esp_result = ESP8266_MqttUserCfg(mqtt_client, "", "");
            if (esp_result >= 0)
                esp_result = ESP8266_MqttConnect(0, mqtt_broker, mqtt_port, 0);
            mqtt_connected = (esp_result == 0) ? 2 : -2;
        }
        else if (cmd == 3) {
            rt_mutex_take(&sensor_mutex, RT_WAITING_FOREVER);
            struct SensorData local = sensor_data;
            rt_mutex_release(&sensor_mutex);
            char payload[64];
            int t_i = (int)local.temp, t_d = (int)((local.temp - t_i) * 10 + 0.5f);
            int h_i = (int)local.humi, h_d = (int)((local.humi - h_i) * 10 + 0.5f);
            if (t_d >= 10) { t_i++; t_d -= 10; }
            if (h_d >= 10) { h_i++; h_d -= 10; }
            snprintf(payload, sizeof(payload),
                "t=%d.%d h=%d.%d l=%u d=%u",
                t_i, t_d, h_i, h_d, local.light, local.distance);
            uart_puts("\r\n[pub] ");
            uart_puts(payload);
            uart_puts("\r\n");
            esp_result = ESP8266_MqttPub("sensor/data", (uint8_t*)payload, strlen(payload));
        }
        else if (cmd == 4) {
            esp_result = ESP8266_MqttSub(0, mqtt_topic, 0);
        }
        else if (cmd == 5) {
            esp_result = ESP8266_MqttUnsub(0, mqtt_topic);
        }
        else if (cmd == 7) {
            esp_result = ESP8266_MqttPub(mqtt_topic, (uint8_t*)"hello", 5);
        }
        else if (cmd == 6) {
#if AUTO_SUB_ECHO
            const char *b = esp_get_buf();
            int len = esp_get_buf_len();
            if (len > 0 && sub_echo) {
                char *p = strstr((char*)b, "+MQTTSUBRECV");
                if (p) {
                    char *eol = strpbrk(p, "\r\n");
                    if (eol) {
                        *eol = '\0';
                        char *last = strrchr(p, ',');
                        if (last) {
                            uart_puts("\r\n[MQTT] ");
                            uart_puts(last + 1);
                            uart_puts("\r\n");
                        }
                    }
                    esp_clear_buf();
                }
            }
            if (len > 512) esp_clear_buf();
#else
            if (esp_get_buf_len() > 512) esp_clear_buf();
#endif

            if (autopub_enabled && mqtt_connected == 2) {
                rt_tick_t now = rt_tick_get();
                if (now - last_autopub >= rt_tick_from_millisecond(AUTOPUB_INTERVAL_S * 1000UL)) {
                    last_autopub = now;
                    rt_mutex_take(&sensor_mutex, RT_WAITING_FOREVER);
                    struct SensorData local = sensor_data;
                    rt_mutex_release(&sensor_mutex);
                    char payload[64];
                    int t_i = (int)local.temp, t_d = (int)((local.temp - t_i) * 10 + 0.5f);
                    int h_i = (int)local.humi, h_d = (int)((local.humi - h_i) * 10 + 0.5f);
                    if (t_d >= 10) { t_i++; t_d -= 10; }
                    if (h_d >= 10) { h_i++; h_d -= 10; }
                    snprintf(payload, sizeof(payload),
                        "t=%d.%d h=%d.%d l=%u d=%u",
                        t_i, t_d, h_i, h_d, local.light, local.distance);
                    ESP8266_MqttPub("sensor/data", (uint8_t*)payload, strlen(payload));
                }
            }
            continue;  /* don't release done_sem for background check */
        }

        rt_sem_release(&esp_done_sem);
    }
}

/* ---- Game Thread (snake on MAX7219) -------------------------------------- */

static void game_thread_entry(void *param)
{
    const uint8_t smiley[8] = {
        0x00, 0x24, 0x24, 0x00, 0x00, 0x42, 0x3C, 0x00
    };
    static int prev_mode = 1;  /* force initial smiley display on boot */

    while (1) {
        if (!game_mode) {
            if (prev_mode) { max7219_display(smiley); prev_mode = 0; }
            rt_thread_mdelay(100);
            continue;
        }
        prev_mode = game_mode;

        if (game_mode == 1) {
            /* serial input: poll for WASD / arrows / Ctrl+C / Enter */
            if (g_serial_snake) {
                int c;
                while ((c = uart_getc()) >= 0) {
                    if (c == 0x03)      { game_mode = 0; break; }       /* Ctrl+C */
                    if (c == '\r' || c == '\n') { game_mode = 0; break; }
                    if (c == 'w' || c == 'W') g_dir = DIR_UP;
                    if (c == 's' || c == 'S') g_dir = DIR_DOWN;
                    if (c == 'a' || c == 'A') g_dir = DIR_LEFT;
                    if (c == 'd' || c == 'D') g_dir = DIR_RIGHT;
                }
                if (!game_mode) { g_serial_snake = 0; continue; }
            }

            snake_tick(g_dir);
            g_dir = DIR_NONE;

            uint8_t rows[8];
            snake_get_display(rows);
            max7219_display(rows);

            if (snake_is_dead()) game_mode = 2;
            rt_thread_mdelay(snake_get_speed_ms());
            continue;
        }

        if (game_mode == 2) {
            const uint8_t all[8]  = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
            const uint8_t none[8] = {0,0,0,0,0,0,0,0};
            int i;
            for (i = 0; i < 3 && game_mode == 2; i++) {
                max7219_display(all);  rt_thread_mdelay(150);
                if (game_mode != 2) break;
                max7219_display(none); rt_thread_mdelay(150);
            }
            if (i >= 3) game_mode = 0;  /* auto-exit after 3 blinks */
            continue;
        }
    }
}

/* ---- Application Entry --------------------------------------------------- */
int main(void)
{
    MX_GPIO_Init();
    MX_I2C1_Init();
    ssd1306_Init(&hi2c1);
    MX_ADC1_Init();
    MX_USART2_UART_Init();
    MX_USART1_UART_Init();
    max7219_init();

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
    HAL_TIM_Base_Start_IT(&htim4);             /* start timer counter */
    HAL_TIM_IC_Start_IT(&htim4, TIM_CHANNEL_2);
    __HAL_TIM_ENABLE_IT(&htim4, TIM_IT_UPDATE);  /* overflow interrupt */

    /* kernel objects (static init — no heap frag) */
    rt_mutex_init(&sensor_mutex, "s_mtx", RT_IPC_FLAG_FIFO);
    rt_mutex_init(&i2c_mutex,    "i_mtx", RT_IPC_FLAG_FIFO);
    rt_sem_init(&sensor_sem, "s_sem", 0, RT_IPC_FLAG_FIFO);
    rt_sem_init(&key_sem,    "k_sem", 0, RT_IPC_FLAG_FIFO);
    rt_sem_init(&esp_go_sem, "e_go",  0, RT_IPC_FLAG_FIFO);
    rt_sem_init(&esp_done_sem,"e_done",0, RT_IPC_FLAG_FIFO);

    /* splash screen */
    ssd1306_Fill(Black);
    /* border */
    for (uint8_t x = 0; x < 128; x++) {
        ssd1306_DrawPixel(x,  0, White);
        ssd1306_DrawPixel(x, 63, White);
    }
    for (uint8_t y = 0; y < 64; y++) {
        ssd1306_DrawPixel(0,   y, White);
        ssd1306_DrawPixel(127, y, White);
    }
    /* title */
    ssd1306_SetCursor(20, 12);
    ssd1306_WriteString("RT-Thread", Font_11x18, White);
    ssd1306_SetCursor(30, 36);
    {
        char ver[16];
        snprintf(ver, sizeof(ver), "v%d.%d.%d", RT_VERSION, RT_SUBVERSION, RT_REVISION);
        ssd1306_WriteString(ver, Font_7x10, White);
    }
    ssd1306_UpdateScreen(&hi2c1);
    rt_thread_mdelay(1500);
    ssd1306_Fill(Black);
    ssd1306_SetCursor(22, 20);
    ssd1306_WriteString("Init...", Font_11x18, White);
    ssd1306_UpdateScreen(&hi2c1);

    rt_thread_t tid;
    tid = rt_thread_create("sens", sensor_thread_entry, RT_NULL, 768,  8, 10);
    if (tid) rt_thread_startup(tid);
    tid = rt_thread_create("oled", oled_thread_entry,   RT_NULL, 896,  12, 10);
    if (tid) rt_thread_startup(tid);
    tid = rt_thread_create("led",  led_thread_entry,    RT_NULL, 512,  11, 10);
    if (tid) rt_thread_startup(tid);
    tid = rt_thread_create("keys", key_thread_entry,    RT_NULL, 384,  10, 10);
    if (tid) rt_thread_startup(tid);
    tid = rt_thread_create("shell", shell_thread_entry,  RT_NULL, 1024, 13, 10);
    if (tid) rt_thread_startup(tid);
    tid = rt_thread_create("esp",   esp_thread_entry,    RT_NULL, 896,  14, 10);
    if (tid) rt_thread_startup(tid);
    tid = rt_thread_create("game",  game_thread_entry,   RT_NULL, 640,   9, 10);
    if (tid) rt_thread_startup(tid);

#if AUTO_WIFI_ENABLE
    if (AUTO_WIFI_SSID[0] != '\0') {
        int ok = 1;
        rt_mutex_take(&i2c_mutex, RT_WAITING_FOREVER);
        ssd1306_Fill(Black);
        ssd1306_SetCursor(10, 24);
        ssd1306_WriteString("Auto WiFi...", Font_11x18, White);
        ssd1306_UpdateScreen(&hi2c1);
        rt_mutex_release(&i2c_mutex);

        strcpy(mqtt_ssid, AUTO_WIFI_SSID);
        strcpy(mqtt_pwd, AUTO_WIFI_PWD);
        esp_cmd = 1;
        rt_sem_release(&esp_go_sem);
        if (rt_sem_take(&esp_done_sem, AUTO_TIMEOUT_WIFI) != RT_EOK || esp_result != 0)
            ok = 0;

        rt_mutex_take(&i2c_mutex, RT_WAITING_FOREVER);
        ssd1306_Fill(Black);
        ssd1306_SetCursor(10, 24);
        ssd1306_WriteString(ok ? "WiFi OK!" : "WiFi FAIL", Font_11x18, White);
        ssd1306_UpdateScreen(&hi2c1);
        rt_mutex_release(&i2c_mutex);
        rt_thread_mdelay(ok ? 500 : 1500);

        if (!ok) goto boot_done;

#if AUTO_MQTT_ENABLE
        if (AUTO_MQTT_IP[0] != '\0') {
            ok = 1;
            rt_mutex_take(&i2c_mutex, RT_WAITING_FOREVER);
            ssd1306_Fill(Black);
            ssd1306_SetCursor(8, 24);
            ssd1306_WriteString("Auto MQTT...", Font_11x18, White);
            ssd1306_UpdateScreen(&hi2c1);
            rt_mutex_release(&i2c_mutex);

            strcpy(mqtt_broker, AUTO_MQTT_IP);
            strcpy(mqtt_port,   AUTO_MQTT_PORT);
            strcpy(mqtt_client, AUTO_MQTT_CLIENT);
            esp_cmd = 2;
            rt_sem_release(&esp_go_sem);
            if (rt_sem_take(&esp_done_sem, AUTO_TIMEOUT_MQTT) != RT_EOK || esp_result != 0)
                ok = 0;

            rt_mutex_take(&i2c_mutex, RT_WAITING_FOREVER);
            ssd1306_Fill(Black);
            ssd1306_SetCursor(8, 24);
            ssd1306_WriteString(ok ? "MQTT OK!" : "MQTT FAIL", Font_11x18, White);
            ssd1306_UpdateScreen(&hi2c1);
            rt_mutex_release(&i2c_mutex);
            rt_thread_mdelay(ok ? 500 : 1500);

            if (!ok) goto boot_done;
        }
#endif

#if AUTO_SUB_ENABLE
        if (AUTO_SUB_TOPIC[0] != '\0') {
            rt_mutex_take(&i2c_mutex, RT_WAITING_FOREVER);
            ssd1306_Fill(Black);
            ssd1306_SetCursor(5, 24);
            ssd1306_WriteString("Auto Sub...", Font_11x18, White);
            ssd1306_UpdateScreen(&hi2c1);
            rt_mutex_release(&i2c_mutex);

            strcpy(mqtt_topic, AUTO_SUB_TOPIC);
            esp_cmd = 4;
            rt_sem_release(&esp_go_sem);
            ok = (rt_sem_take(&esp_done_sem, AUTO_TIMEOUT_SUB) == RT_EOK && esp_result == 0);
            if (ok) mqtt_sub_active = 1;

            rt_mutex_take(&i2c_mutex, RT_WAITING_FOREVER);
            ssd1306_Fill(Black);
            ssd1306_SetCursor(5, 24);
            ssd1306_WriteString(ok ? "Sub OK!" : "Sub FAIL", Font_11x18, White);
            ssd1306_UpdateScreen(&hi2c1);
            rt_mutex_release(&i2c_mutex);
            rt_thread_mdelay(500);
        }
#endif
    }
boot_done:
    rt_mutex_take(&i2c_mutex, RT_WAITING_FOREVER);
    ssd1306_Fill(Black);
    ssd1306_UpdateScreen(&hi2c1);
    rt_mutex_release(&i2c_mutex);
#endif

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
