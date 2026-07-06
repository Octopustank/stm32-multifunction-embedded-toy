# Project-rttnano 项目结构

STM32F103C8T6 (Cortex-M3, 64KB Flash, 20KB RAM) — RT-Thread Nano + STM32 HAL

## 目录总览

```
Project-rttnano/
├── Core/                           # 用户应用层
│   ├── Inc/
│   │   ├── main.h                  # 引脚宏定义、函数声明
│   │   ├── gpio.h                  # GPIO 初始化声明
│   │   ├── i2c.h                   # I2C1 初始化 (PB8/PB9, SSD1306)
│   │   ├── adc.h                   # ADC1 初始化 (PA0, 光照)
│   │   ├── tim.h                   # TIM4 输入捕获 (PB7, SR04)
│   │   ├── usart.h                 # USART1/2 初始化 + 环形缓冲
│   │   ├── stm32f1xx_hal_conf.h    # HAL 模块开关
│   │   ├── stm32f1xx_it.h          # 中断 handler 声明
│   │   └── rtconfig.h              # RTT-Nano 内核配置
│   │   └── net_auto_config.h         # 网络自动配置 (由 make menuconfig 生成)
│   └── Src/
│       ├── main.c                  # RTT 应用入口 (6 个线程)
│       ├── board.c                 # RTT 板级初始化 (HAL + 时钟 + SysTick + 堆)
│       ├── gpio.c                  # GPIO 初始化 (LED / 按键)
│       ├── i2c.c                   # I2C1 初始化 + MSP (PB8=SCL, PB9=SDA, 100kHz, remap)
│       ├── adc.c                   # ADC1 初始化 + 单次采样 (PA0=CH0, 12bit)
│       ├── tim.c                   # TIM4 CH2 输入捕获 + ISR (SR04 Echo)
│       ├── usart.c                 # USART1 (ESP8266) + USART2 (Shell) 初始化
│       ├── stm32f1xx_hal_msp.c     # HAL MSP 初始化 (空)
│       ├── stm32f1xx_it.c          # 异常/外设中断 handler
│       ├── system_stm32f1xx.c      # CMSIS 系统初始化
│       ├── sysmem.c                # newlib _sbrk (RTT 接管后不再使用)
│       └── syscalls.c              # newlib 系统调用桩
│
├── Libraries/                      # 外设驱动库
│   ├── SSD1306/
│   │   ├── ssd1306.h / .c          # SSD1306 OLED 驱动 (I2C, 128x64, 3 种字体)
│   │   └── fonts.h / .c            # 字体位图 (7x10 / 11x18 / 16x26)
│   ├── DHT11/
│   │   ├── dht11.h / .c            # DHT11 单总线温湿度 (PB5, 超时保护)
│   │   └── delay.h / .c            # DWT 周期计数器微秒延时 (无外设依赖)
│   ├── SR04/
│   │   └── HCSR04.h / .c           # HC-SR04 超声波测距 (B6=Trig, B7=Echo)
│   ├── MAX7219/
│   │   └── max7219.h / .c          # MAX7219 8x8 LED 矩阵 (软 SPI: PA11=DIN, PA12=CLK, PA15=CS)
│   ├── SNAKE/
│   │   └── snake.h / .c            # 贪吃蛇游戏引擎 (纯 C, 无 HAL/RTOS 依赖)
│   └── ESP8266_MQTT/
│       └── esp8266_mqtt.h / .c     # ESP8266 AT 命令驱动 (USART1, MQTT 支持)
│
├── Drivers/                        # 硬件驱动层 (只读, CubeMX 生成)
│   ├── CMSIS/
│   └── STM32F1xx_HAL_Driver/
│
├── rtthread-nano/                  # RT-Thread Nano 内核
│
├── startup_stm32f103xb.s           # 启动文件 (bl main → bl entry)
├── STM32F103XX_FLASH.ld            # 链接脚本 (Flash 64K, RAM 20K)
├── Makefile                        # 构建 (arm-none-eabi-gcc, -mcpu=cortex-m3)
├── Kconfig                          # menuconfig 配置项定义 (网络自动配置)
├── Config/                          # 配置工具
│   ├── menuconfig.py                # Kconfiglib menuconfig (curses UI)
│   └── gen_header.py                # .config → net_auto_config.h 生成器
├── Project.ioc                     # CubeMX 工程文件
└── build/                          # 构建产物
```

## 引脚占用

| 引脚 | 模式 | 外设 | 说明 |
|------|------|------|------|
| PB13 | OUT PP | LED1 | 表情灯 (think 流水, happy/sad 全亮) |
| PB14 | OUT PP | LED2 | 表情灯 |
| PB15 | OUT PP | LED3 | 表情灯 |
| PA8  | OUT PP | LED4 | 表情灯 |
| PB10 | IN PU | K1_LEFT | 按键 / 蛇左 |
| PB0  | IN PU | K2_RIGHT | 按键 / 蛇右 |
| PB11 | IN PU | K3_UP | 按键 / 蛇上 / 进游戏 |
| PB12 | IN PU | K4_DOWN | 按键 / 蛇下 |
| PB1  | IN PU | K5_CENTER | 按键 / 退出/重开 |
| PB8  | AF OD | I2C1_SCL | OLED (remap from PB6) |
| PB9  | AF OD | I2C1_SDA | OLED (remap from PB7) |
| PB5  | I/O | DHT11 | 单总线温湿度 |
| PA0  | Analog | ADC1_IN0 | 光照传感器 |
| PA2  | AF PP | USART2_TX | Shell 终端 (115200) |
| PA3  | IN | USART2_RX | Shell 终端 |
| PA9  | AF PP | USART1_TX | ESP8266 |
| PA10 | IN | USART1_RX | ESP8266 |
| PA11 | OUT PP | MAX7219 DIN | 软 SPI 数据 |
| PA12 | OUT PP | MAX7219 CLK | 软 SPI 时钟 |
| PA15 | OUT PP | MAX7219 CS | 软 SPI 片选 |
| PB6  | OUT PP | SR04 Trig | 超声波触发 |
| PB7  | IN | TIM4_CH2 | 超声波回波 |

## 线程表

| 线程 | 栈 | 优先级 | 职责 |
|------|------|-------|------|
| sens | 768B | 8 | DHT11(800ms) + ADC + SR04 → 黑板写入 |
| keys | 384B | 10 | 按键扫描 + 蛇方向 / 启动 |
| oled | 896B | 12 | 黑板读取 + OLED 三行显示 |
| shell | 1024B | 13 | UART 命令解析 (历史) |
| esp | 896B | 14 | WiFi/MQTT 异步 worker + 轮询收包 + 自动发布 |
| game | 640B | 9 | 贪吃蛇 + 表情显示 + LED 控制 (MAX7219) |

## 架构设计

### 黑板模式 (传感器 → 多消费者)

```
sensor_thread (prio 8)  → sensor_data (sensor_mutex) → oled_thread (prio 12)
                                  ↕ sensor_sem
                          未来: MQTT 消费者 (prio 14)
```

### 表情黑板 (Shell/MQTT → game_thread)

```
shell / MQTT subexec  → face_mode (face_mutex)  → game_thread (prio 9)
     (producer)                                       (consumer: MAX7219 + LED)
```

表情模式: IDLE (笑脸) / THINK (点阵 think1⇄2 + LED 流水) / HAPPY (笑脸点阵 + LED 全亮) / SAD (不开心点阵 + LED 全亮)

### ESP8266 异步架构

```
shell (13) ──wifi/connect/mqtt/pub──→ esp_go_sem → esp_thread (14)
   │                                                     │
   └── esp_done_sem ← 阻塞等待结果 ←─────────────── AT命令 + MQTT轮询 + AutoPub
```

命令分离：`wifi` 仅保存凭据，`connect` 触发连接。每步阻塞等待结果（带超时）。esp 线程空闲时轮询 MQTT 收包（subecho/subexec/autopub），不污染终端。

### 游戏状态机

```
IDLE → (K3) → GAME → (撞墙) → DEAD
                ↑                ↓ K3=重开
                └────────────────┘
K5=退出
```

## Shell 命令

```
help                  # 帮助
stat                  # 多行仪表盘 (传感器 + WiFi/MQTT 状态 + 订阅 + toggles)
snake                 # 贪吃蛇 (WASD 方向, Enter 结束)
happy                 # 点阵笑脸, LED 全亮
sad                   # 点阵不开心, LED 全亮
think                 # 点阵 think1⇄2 交替 + LED 流水
autopub               # 切换后台自动发布
subecho               # 切换 MQTT 订阅消息转发到终端 (需 AUTO_SUB_ECHO=y)
subexec               # 切换 MQTT 订阅消息注入 Shell 执行 (需 AUTO_SUB_EXEC=y)

wifi SSID PWD         # 保存 AP 凭据
connect               # WiFi 连接 (阻塞 25s)
mqtt IP PORT ID       # 保存 MQTT 配置
mqttconn              # MQTT 连接 (阻塞 15s)
pub                   # 发送传感器数据
sub TOPIC             # 订阅主题 (后台自动打印)
unsub                 # 取消当前订阅
test TOPIC            # 发送 "hello" 调试

↑↓ 浏览历史
```

## 关键设计决策

1. **黑板模式** — 共享 SensorData + mutex + sem，支持多消费者
2. **DHT11 无锁** — 超时保护替代关中断，SysTick 可打断，~4% 误读由 800ms 高频补偿
3. **DWT 微秒延时** — Cortex-M3 周期计数器，零外设依赖
4. **表情黑板** — Shell/MQTT 写入 face_mode + mutex，game_thread 空闲时读取显示，解耦命令与渲染
5. **信号量替代轮询** — `rt_sem_take(timeout)` 代替 `delay_yield`、`rt_sem` 握手代替 `esp_cmd` 轮询
6. **非阻塞按键** — 下降沿检测 + debounce 状态机，多键并发
7. **I2C1 remap** — PB8/PB9，STM32F103 重映射
8. **MAX7219 软 SPI** — 寄存器直写 (BSRR/BRR)，免硬件 SPI 冲突
9. **ESP8266 异步** — 配置/动作分离，信号量握手，终端零阻塞；空闲时轮询 MQTT 收包、自动发布
10. **MQTT 接收自动打印** — esp 线程轮询 +MQTTSUBRECV，可切换 subecho/subexec 模式
11. **WaitResponse 容忍订阅流** — 每 2ms 重试替代静默等待，订阅消息不干扰 AT 命令
12. **mqtt 线程合并** — 原独立 mqtt 线程并入 esp 线程，减少线程切换开销

## 外设模块

| 模块 | HAL 宏 | 说明 |
|------|--------|------|
| GPIO | `HAL_GPIO_MODULE_ENABLED` | LED + 按键 + DHT11 + MAX7219 |
| I2C | `HAL_I2C_MODULE_ENABLED` | OLED SSD1306 |
| ADC | `HAL_ADC_MODULE_ENABLED` | 光照传感器 PA0 |
| TIM | `HAL_TIM_MODULE_ENABLED` | SR04 输入捕获 + DHT11 超时 |
| UART | `HAL_UART_MODULE_ENABLED` | Shell (USART2) + ESP8266 (USART1) |
| RCC/DMA/Cortex/PWR/Flash/EXTI | — | 基础模块 |

## 构建

```bash
make menuconfig           # 交互式配置网络自动连接参数 (Kconfig)
make genconfig            # 从 .config 重新生成 net_auto_config.h
make -j$(nproc)           # 编译
make flash                # 烧录 (OpenOCD + ST-Link)
make clean                # 清理

# ~40KB text, ~14KB bss  (64KB/20KB)
build/Project.bin
```

## 网络自动配置

开机自动联网 / MQTT / 订阅 / 发布由 `make menuconfig` (Kconfig) 控制：

```bash
make menuconfig           # 打开 curses UI 配置
make genconfig            # 手动重新生成 net_auto_config.h
make                      # 直接编译 (头文件已是最新)
```

配置项保存在 `.config`，生成的头文件 `Core/Inc/net_auto_config.h` **不要手动编辑**。

依赖关系：
- WiFi → MQTT → Sub / AutoPub
- 任一步失败则停止后续步骤，OLED 显示 FAIL → 继续正常启动

运行时命令：
- `autopub` — 切换后台弱周期自动发布 (每 N 秒, N 由 menuconfig 配置)
- `subecho` — 切换 MQTT 订阅消息转发到终端 (仅在 `AUTO_SUB_ECHO=y` 时编译)
- `subexec` — 切换 MQTT 订阅消息注入 Shell 执行 (仅在 `AUTO_SUB_EXEC=y` 时编译)
- `stat` — 显示所有传感器数据、WiFi/MQTT 状态、订阅和 toggle 开关
- 手动 `wifi` / `connect` / `mqtt` / `mqttconn` / `sub` / `pub` 始终可用

预编译控制：`#if AUTO_XXX` 宏在功能禁用时完全消除代码 (WiFi/MQTT/Sub/SubEcho)，零 flash/RAM 开销。

## Git 历史

```
a051079 feat: add snake game on MAX7219
1655cc8 feat: add ESP8266 MQTT support via USART1
b294335 feat: add RT-Thread version banner on shell startup
1fe62b7 feat: shell history + Tab autocomplete + RTT logo
86c643a fix: replace %f with manual int.frac in shell
242e52e feat: add UART2 shell
688f7ac fix: add HAL_TIM_Base_Start_IT for SR04
3591a3e fix: add timeout wait in getSR04Distance
2cc6084 feat: add SR04 ultrasonic sensor
abe6bfd refactor: Blackboard pattern + semaphore architecture
e552195 feat: add light sensor on PA0
4ed8475 feat: add I2C OLED and DHT11 sensor
```
