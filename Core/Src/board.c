/*
 * board.c - RT-Thread Nano board support for STM32F103C8T6
 *
 * Implements rt_hw_board_init() for:
 *   - HAL initialization
 *   - Clock configuration (72MHz from HSE+PLL)
 *   - SysTick setup for OS tick
 *   - Heap initialization
 */

#include <rthw.h>
#include <rtthread.h>
#include "main.h"

/* ---- Heap --------------------------------------------------------------- */

#if defined(RT_USING_USER_MAIN) && defined(RT_USING_HEAP)
/*
 * Reserve 8 KB for RT-Thread dynamic heap.
 * Total RAM is 20 KB. After .bss/.data, main thread stack (512),
 * and other thread stacks, 8 KB heap is safe.
 */
#define RT_HEAP_SIZE    (8 * 1024)
static rt_uint8_t rt_heap[RT_HEAP_SIZE];

void *rt_heap_begin_get(void)
{
    return rt_heap;
}

void *rt_heap_end_get(void)
{
    return rt_heap + RT_HEAP_SIZE;
}
#endif /* RT_USING_USER_MAIN && RT_USING_HEAP */

/* ---- OS Tick Callback --------------------------------------------------- */

/**
 * @brief  Called from SysTick_Handler to increment the OS tick.
 */
static void rt_os_tick_callback(void)
{
    rt_interrupt_enter();
    HAL_IncTick();          /* keep HAL timebase alive */
    rt_tick_increase();     /* RT-Thread kernel tick */
    rt_interrupt_leave();
}

/* ---- Board Initialization ----------------------------------------------- */

/**
 * @brief  RT-Thread board-level initialization.
 *         Called by rtthread_startup() before the scheduler starts.
 */
void rt_hw_board_init(void)
{
    /* HAL initialization */
    HAL_Init();

    /* Configure system clock (72 MHz from HSE + PLL) */
    SystemClock_Config();

    /* Update CMSIS variable */
    SystemCoreClockUpdate();

    /* Configure SysTick for OS tick */
    SysTick_Config(SystemCoreClock / RT_TICK_PER_SECOND);

    /* Initialize components with INIT_BOARD_EXPORT() */
#ifdef RT_USING_COMPONENTS_INIT
    rt_components_board_init();
#endif

    /* Initialize heap for dynamic thread/IPC creation */
#if defined(RT_USING_USER_MAIN) && defined(RT_USING_HEAP)
    rt_system_heap_init(rt_heap_begin_get(), rt_heap_end_get());
#endif
}

/* ---- SysTick Handler ---------------------------------------------------- */

/**
 * @brief  SysTick interrupt handler.
 *         Overrides the weak alias in startup_stm32f103xb.s.
 *         MUST be removed from stm32f1xx_it.c to avoid duplicate symbol.
 */
void SysTick_Handler(void)
{
    rt_os_tick_callback();
}
