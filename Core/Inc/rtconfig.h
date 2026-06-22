/*
 * RT-Thread Nano Configuration for STM32F103C8T6
 */

#ifndef __RTTHREAD_CFG_H__
#define __RTTHREAD_CFG_H__

/* ========== Basic ========== */
#define RT_THREAD_PRIORITY_MAX      32
#define RT_TICK_PER_SECOND          1000
#define RT_ALIGN_SIZE               4
#define RT_NAME_MAX                 8

#define RT_USING_COMPONENTS_INIT
#define RT_USING_USER_MAIN

/* main thread stack: increase from default 256 for safety */
#define RT_MAIN_THREAD_STACK_SIZE   512

/* ========== IPC ========== */
#define RT_USING_SEMAPHORE

/* ========== Memory ========== */
#define RT_USING_HEAP
#define RT_USING_SMALL_MEM
#define RT_USING_SMALL_MEM_AS_HEAP

/* ========== Idle Hook (for LED debugging) ========== */
/* #define RT_USING_IDLE_HOOK */

#endif /* __RTTHREAD_CFG_H__ */
