# Project-rttnano 项目结构

STM32F103C8T6 (Cortex-M3, 64KB Flash, 20KB RAM) — RT-Thread Nano + STM32 HAL

## 目录总览

```
Project-rttnano/
├── Core/                           # 用户应用层
│   ├── Inc/
│   │   ├── main.h                  # 引脚宏定义、函数声明
│   │   ├── gpio.h                  # GPIO 初始化声明
│   │   ├── i2c.h                   # I2C1 初始化声明 (PB8/PB9, SSD1306)
│   │   ├── adc.h                   # ADC1 初始化声明 (PA0, 光照传感器)
│   │   ├── stm32f1xx_hal_conf.h    # HAL 模块开关 (GPIO / I2C / ADC / DMA / RCC ...)
│   │   ├── stm32f1xx_it.h          # 中断 handler 声明
│   │   └── rtconfig.h              # RTT-Nano 内核配置
│   └── Src/
│       ├── main.c                  # RTT 应用入口 (LED + 按键 + OLED/DHT11/光照 线程)
│       ├── board.c                 # RTT 板级初始化 (HAL + 时钟 + SysTick + 堆)
│       ├── gpio.c                  # GPIO 初始化 (LED / 按键)
│       ├── i2c.c                   # I2C1 初始化 + MSP (PB8=SCL, PB9=SDA, 100kHz, remap)
│       ├── adc.c                   # ADC1 初始化 + 单次采样 (PA0=CH0, 12bit)
│       ├── stm32f1xx_hal_msp.c     # HAL MSP 初始化 (空)
│       ├── stm32f1xx_it.c          # 异常 handler (HardFault/PendSV/SysTick 已移除)
│       ├── system_stm32f1xx.c      # CMSIS 系统初始化
│       ├── sysmem.c                # newlib _sbrk (RTT 接管后不再使用)
│       └── syscalls.c              # newlib 系统调用桩
│
├── Libraries/                      # 外设驱动库 (从 Arch 引入)
│   ├── SSD1306/
│   │   ├── ssd1306.h / .c          # SSD1306 OLED 驱动 (I2C, 128x64, 3 种字体)
│   │   └── fonts.h / .c            # 字体位图数据 (7x10 / 11x18 / 16x26)
│   └── DHT11/
│       ├── dht11.h / .c            # DHT11 单总线温湿度驱动 (PB5)
│       └── delay.h / .c            # DWT 周期计数器微秒延时 (无外设依赖)
│
├── Drivers/                        # 硬件驱动层 (只读, CubeMX 生成)
│   ├── CMSIS/
│   └── STM32F1xx_HAL_Driver/
│       ├── Inc/                    # HAL 头文件
│       └── Src/                    # HAL 源码 (编译时链接用到的模块)
│
├── rtthread-nano/                  # RT-Thread Nano v3.1.x 内核
│   └── rt-thread/
│       ├── src/                    # 内核源码 (12 个 .c)
│       ├── include/                # 内核头文件
│       └── libcpu/arm/cortex-m3/   # Cortex-M3 移植层
│
├── startup_stm32f103xb.s           # 启动文件 (修改: bl main → bl entry)
├── STM32F103XX_FLASH.ld            # 链接脚本 (Flash 64K, RAM 20K)
├── Makefile                        # 构建 (arm-none-eabi-gcc, -mcpu=cortex-m3)
├── Project.ioc                     # CubeMX 工程文件
└── build/                          # 构建产物 (.o, .elf, .hex, .bin)
```

## 引脚占用

| 引脚 | 模式 | 外设 | 说明 |
|------|------|------|------|
| PB13 | OUT PP | LED1 | 流水灯 |
| PB14 | OUT PP | LED2 | 流水灯 |
| PB15 | OUT PP | LED3 | 流水灯 |
| PA8  | OUT PP | LED4 | 流水灯 |
| PB10 | IN PU | K1_LEFT | 按键 |
| PB0  | IN PU | K2_RIGHT | 按键 |
| PB11 | IN PU | K3_UP | 按键 |
| PB12 | IN PU | K4_DOWN | 按键 |
| PB1  | IN PU | K5_CENTER | 按键 (模式切换) |
| PB8  | AF OD | I2C1_SCL | OLED (remap from PB6) |
| PB9  | AF OD | I2C1_SDA | OLED (remap from PB7) |
| PB5  | I/O | DHT11 | 单总线温湿度 |
| PA0  | Analog | ADC1_IN0 | 光照传感器 |

## 三层架构

```
┌──────────────────────────────────────────┐
│  Core/ + Libraries/  应用层               │
│  main.c             4 个线程              │
│  i2c.c / adc.c      外设初始化            │  ← 业务代码
│  Libraries/          SSD1306 / DHT11     │
├──────────────────────────────────────────┤
│  rtthread-nano/     中间件层              │
│  RT-Thread Nano     内核                  │  ← 不可修改
├──────────────────────────────────────────┤
│  Drivers/            硬件驱动层            │
│  CMSIS + HAL         GPIO/I2C/ADC/DMA... │  ← CubeMX 生成
└──────────────────────────────────────────┘
```

## 启动流程

```
硬件复位
  └→ startup_stm32f103xb.s  Reset_Handler
       ├→ SystemInit()                          CMSIS 系统初始化
       ├→ 拷贝 .data 段, 清零 .bss
       └→ bl entry                              RTT-Nano 入口
            └→ entry()
                 └→ rtthread_startup()
                      ├→ rt_hw_board_init()     Core/Src/board.c
                      │    ├→ HAL_Init()
                      │    ├→ SystemClock_Config()   72MHz (HSE 8M × PLL9)
                      │    ├→ SysTick_Config()       1KHz OS tick
                      │    └→ rt_system_heap_init()  8KB 堆
                      ├→ rt_application_init()   创建 main 线程 (512B, prio 10)
                      ├→ rt_thread_idle_init()   创建 idle 线程
                      └→ rt_system_scheduler_start()
                           └→ main()             Core/Src/main.c
                                ├→ MX_GPIO_Init()
                                ├→ MX_I2C1_Init() + ssd1306_Init()
                                ├→ MX_ADC1_Init()
                                ├→ 创建 oled 线程 (768B, prio 12)
                                ├→ 创建 led 线程  (512B, prio 10)
                                ├→ 创建 keys 线程 (384B, prio 11)
                                └→ 挂起 (rt_thread_mdelay forever)
```

## 线程表

| 线程 | 栈 | 优先级 | 职责 |
|------|------|-------|------|
| main | 512B | 10 | 初始化外设、创建子线程 |
| led | 512B | 10 | 自动模式: 流水灯 + 全闪动画 |
| keys | 384B | 11 | 唯一硬件 PIN 读取者 / 模式切换 |
| oled | 768B | 12 | DHT11 温湿度 + ADC 光照 → OLED 显示 (2.5s 刷新) |
| idle | 自动 | 31 | RTT 内置空闲 |

## 关键设计决策

1. **硬件引脚只由 key 线程读取** — LED 线程通过 `volatile` flag 响应，消除竞态
2. **SysTick 同时服务 HAL 和 RTT** — `SysTick_Handler` 中调用 `HAL_IncTick()` + `rt_tick_increase()`
3. **DHT11 临界区保护** — `__disable_irq()` 锁定 ~5ms 位拆阶段，防止 RTOS 抢占破坏 μs 时序
4. **DWT 微秒延时** — 用 Cortex-M3 硬件周期计数器替代 TIM2，无外设依赖
5. **DHT11 首次读丢弃** — 上电后传感器需稳定 ~2s，首次数据不可靠
6. **I2C 总线程独占** — 仅 oled 线程使用 I2C，无需 mutex
7. **I2C1 remap** — PB8/PB9 通过 `__HAL_AFIO_REMAP_I2C1_ENABLE()` 重映射
8. **OLED 字体自适应** — 数据用 Font_7x10 (18 字符/行)，避免 128px 截断
9. **所有 HAL_Delay 替换为 rt_thread_mdelay** — 让出 CPU

## 外设模块

| 模块 | HAL 宏 | 源码 | 说明 |
|------|--------|------|------|
| GPIO | `HAL_GPIO_MODULE_ENABLED` | `stm32f1xx_hal_gpio.c` | LED + 按键 |
| I2C | `HAL_I2C_MODULE_ENABLED` | `stm32f1xx_hal_i2c.c` | OLED SSD1306 |
| ADC | `HAL_ADC_MODULE_ENABLED` | `stm32f1xx_hal_adc.c` | 光照传感器 |
| RCC | `HAL_RCC_MODULE_ENABLED` | `stm32f1xx_hal_rcc.c` | 时钟树 |
| DMA | `HAL_DMA_MODULE_ENABLED` | `stm32f1xx_hal_dma.c` | (预留) |
| Cortex | `HAL_CORTEX_MODULE_ENABLED` | `stm32f1xx_hal_cortex.c` | NVIC |
| PWR | `HAL_PWR_MODULE_ENABLED` | `stm32f1xx_hal_pwr.c` | 电源 |
| Flash | `HAL_FLASH_MODULE_ENABLED` | `stm32f1xx_hal_flash.c` | Flash 等待周期 |
| EXTI | `HAL_EXTI_MODULE_ENABLED` | `stm32f1xx_hal_exti.c` | (预留) |

## 构建

```bash
make -j$(nproc)          # 编译
make flash               # 烧录 (OpenOCD + ST-Link)
make clean               # 清理

# 最新构建产物
build/Project.elf        # ~26KB text, ~12KB bss  (64KB/20KB 余量充足)
build/Project.bin
build/Project.hex
```

## 修改过的 CubeMX 生成文件

| 文件 | 修改内容 |
|------|---------|
| `startup_stm32f103xb.s` | `bl main` → `bl entry` (调用 RTT 入口) |
| `Core/Src/stm32f1xx_it.c` | 移除 `HardFault_Handler` / `PendSV_Handler` / `SysTick_Handler` |
| `Core/Src/main.c` | 重写为 RTT 多线程 |
| `Core/Inc/main.h` | 添加引脚定义、`SystemClock_Config()` 声明 |
| `Core/Inc/stm32f1xx_hal_conf.h` | 启用 I2C / ADC 模块 |

> **注意**：用 CubeMX 重新生成代码时，以上文件会被覆盖，需手动恢复。

## Git 历史

```
e552195 feat: add light sensor on PA0 (ADC1 channel 0)
4ed8475 feat: add I2C OLED (SSD1306) and DHT11 sensor
```
