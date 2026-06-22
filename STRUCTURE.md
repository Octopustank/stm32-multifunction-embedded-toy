# Project-rttnano 项目结构

STM32F103C8T6 (Cortex-M3, 64KB Flash, 20KB RAM) — RT-Thread Nano + STM32 HAL

## 目录总览

```
Project-rttnano/
├── Core/                           # 用户应用层（HAL 配置 + RTT 适配 + 业务逻辑）
│   ├── Inc/
│   │   ├── main.h                  # 引脚宏定义、函数声明
│   │   ├── gpio.h                  # GPIO 初始化声明
│   │   ├── stm32f1xx_hal_conf.h    # HAL 模块开关（CubeMX 生成）
│   │   ├── stm32f1xx_it.h          # 中断 handler 声明
│   │   └── rtconfig.h              # RTT-Nano 内核配置（时钟节拍、堆大小、IPC 开关）
│   └── Src/
│       ├── main.c                  # RTT 应用入口（LED 线程 + 按键线程）
│       ├── board.c                 # RTT 板级初始化（HAL + 时钟 + SysTick + 堆）
│       ├── gpio.c                  # GPIO 初始化（CubeMX 生成）
│       ├── stm32f1xx_hal_msp.c     # HAL MSP 初始化（CubeMX 生成）
│       ├── stm32f1xx_it.c          # 异常/中断 handler（CubeMX 生成，部分已移除）
│       ├── system_stm32f1xx.c      # CMSIS 系统初始化
│       ├── sysmem.c                # newlib _sbrk 堆管理（RTT 接管后不再使用）
│       └── syscalls.c              # newlib 系统调用桩
│
├── Drivers/                        # 硬件驱动层（只读，CubeMX 生成）
│   ├── CMSIS/
│   │   ├── Core/Include/           # CMSIS-Core 头文件（Cortex-M3 寄存器定义）
│   │   ├── Include/                # CMSIS 通用头（core_cm3.h 等）
│   │   └── Device/ST/STM32F1xx/    # STM32F1 设备头 + 启动文件模板
│   └── STM32F1xx_HAL_Driver/
│       ├── Inc/                    # HAL 头文件（61 个 .h）
│       └── Src/                    # HAL 源码（61 个 .c，编译时只链接用到的 11 个）
│
├── Middlewares/                    # 中间件层
│   └── RT-Thread/                  # RT-Thread Nano v3.1.x
│       ├── include/                # 内核头文件（rtthread.h, rtdef.h, rthw.h…）
│       ├── *.c                     # 内核源码（13 个编译文件，4 个未启用）
│       └── libcpu/arm/cortex-m3/   # Cortex-M3 移植层
│           ├── cpuport.c           # 中断开关、HardFault 处理、栈初始化
│           ├── context_gcc.S       # PendSV 上下文切换（GCC 汇编）
│           ├── context_iar.S       # （IAR 备用）
│           └── context_rvds.S      # （MDK 备用）
│
├── startup_stm32f103xb.s           # 启动文件（修改：bl main → bl entry）
├── STM32F103XX_FLASH.ld            # 链接脚本（Flash 64K @ 0x08000000, RAM 20K @ 0x20000000）
├── Makefile                        # 构建脚本（arm-none-eabi-gcc, -mcpu=cortex-m3）
├── Project.ioc                     # CubeMX 工程文件（外设引脚映射）
└── build/                          # 构建产物（.o, .elf, .hex, .bin）
```

## 三层架构

```
┌──────────────────────────────────────────┐
│  Core/         应用层                    │
│  main.c        LED 线程 + Key 线程       │  ← 你的业务代码
│  board.c       RTT 板级适配              │
│  rtconfig.h    RTT 配置                  │
├──────────────────────────────────────────┤
│  Middlewares/  中间件层                  │
│  RT-Thread/    RT-Thread Nano 内核       │  ← 从 GitHub 引入，不可修改
├──────────────────────────────────────────┤
│  Drivers/      硬件驱动层                │
│  CMSIS/        ARM 标准接口              │  ← CubeMX 生成，不可修改
│  STM32F1xx_    HAL 外设驱动              │
│  HAL_Driver/                             │
└──────────────────────────────────────────┘
```

## 启动流程

```
硬件复位
  └→ startup_stm32f103xb.s  Reset_Handler
       ├→ SystemInit()                          CMSIS 系统初始化
       ├→ 拷贝 .data 段, 清零 .bss
       └→ bl entry                              RTT-Nano 入口（修改处）
            └→ entry()                           Middlewares/RT-Thread/components.c
                 └→ rtthread_startup()
                      ├→ rt_hw_board_init()      Core/Src/board.c
                      │    ├→ HAL_Init()
                      │    ├→ SystemClock_Config()   72MHz (HSE 8M × PLL9)
                      │    ├→ SysTick_Config()       1KHz OS tick
                      │    └→ rt_system_heap_init()  8KB 堆
                      ├→ rt_application_init()   创建 main 线程 (512B 栈, prio 10)
                      ├→ rt_thread_idle_init()   创建 idle 线程
                      └→ rt_system_scheduler_start()  调度器启动（永不返回）
                           └→ main()             Core/Src/main.c
                                ├→ MX_GPIO_Init()
                                ├→ 创建 led 线程  (512B, prio 10)
                                ├→ 创建 keys 线程 (384B, prio 11)
                                └→ 挂起 (rt_thread_mdelay forever)
```

## 线程表

| 线程 | 栈 | 优先级 | 文件 | 职责 |
|------|------|-------|------|------|
| main | 512B | 10 | main.c | 初始化外设、创建子线程 |
| led | 512B | 10 | main.c | 自动模式：流水灯 + 全闪动画 |
| keys | 384B | 11 | main.c | 唯一硬件 PIN 读取者 / 模式切换 / 手动选灯 |
| idle | 自动 | 31 | idle.c | RTT 内置空闲线程 |

## 关键设计决策

1. **硬件引脚只由 key 线程读取** — 消除竞态条件，LED 线程通过 `volatile` flag 响应中键
2. **SysTick 被 RTT 接管** — `SysTick_Handler` 中同时调用 `HAL_IncTick()` 保持 HAL 时钟
3. **PendSV/SVC 归 RTT** — 原 `stm32f1xx_it.c` 中的 handler 已移除，由 `context_gcc.S` 提供
4. **RT_USING_HEAP 开启** — 8KB 静态数组作为 RTT 动态内存池
5. **所有 `HAL_Delay` 替换为 `rt_thread_mdelay`** — 让出 CPU 给调度器

## 构建

```bash
make -j$(nproc)          # 编译
make flash               # 烧录 (OpenOCD + ST-Link)
make clean               # 清理

# 产物
build/Project.elf        # 10.2KB text, 10.5KB bss  (64KB/20KB 余量充足)
build/Project.bin
build/Project.hex
```

## 修改过的 CubeMX 生成文件

| 文件 | 修改内容 |
|------|---------|
| `startup_stm32f103xb.s` | `bl main` → `bl entry`（调用 RTT 入口） |
| `Core/Src/stm32f1xx_it.c` | 移除 `HardFault_Handler` / `PendSV_Handler` / `SysTick_Handler` |
| `Core/Src/main.c` | 重写为 RTT 多线程 |
| `Core/Inc/main.h` | 添加 `SystemClock_Config()` 声明 |

> **注意**：用 CubeMX 重新生成代码时，以上文件会被覆盖，需手动恢复。
