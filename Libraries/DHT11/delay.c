#include "delay.h"

/* DWT (Data Watchpoint and Trace) cycle counter — no peripheral init needed.
 * At 72 MHz: 72 cycles = 1 us */
#define DWT_DELAY_CYCLES_PER_US  72U

static uint8_t dwt_ready = 0;

void delay_init(void)
{
    if (!dwt_ready) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
        dwt_ready = 1;
    }
}

void delayus(uint16_t us)
{
    if (!dwt_ready)
        delay_init();

    uint32_t start = DWT->CYCCNT;
    uint32_t target = us * DWT_DELAY_CYCLES_PER_US;

    while ((DWT->CYCCNT - start) < target)
        ;
}
