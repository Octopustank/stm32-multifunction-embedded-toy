#ifndef __TIM_H__
#define __TIM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

extern TIM_HandleTypeDef htim4;

/* SR04 echo capture state:
 *   bit 7 : capture complete
 *   bit 5-0: overflow count */
extern uint8_t  TIM4CH2_CAPTURE_STA;
extern uint16_t TIM4CH2_CAPTURE_VAL;

void MX_TIM4_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __TIM_H__ */
