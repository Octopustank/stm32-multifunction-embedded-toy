#ifndef __HCSR04_H
#define __HCSR04_H

#include "stdint.h"

extern uint8_t  TIM4CH2_CAPTURE_STA;
extern uint16_t TIM4CH2_CAPTURE_VAL;

void HCSR_04(void);
float getSR04Distance(void);

#endif
