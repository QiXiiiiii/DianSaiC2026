#ifndef __BSP_BASIC_TIM6_H
#define	__BSP_BASIC_TIM6_H

#include "stm32f1xx.h"

extern TIM_HandleTypeDef htim6;

/*
72 000 000 / 3600 / 100  = 200Hz (5ms)
*/

#define TIM6_DEF_PRESCALER   3600 - 1
#define TIM6_DEF_PERIOD      100  - 1

void TIM6_Config(void);



#endif /* __BSP_BASIC_TIM6_H */
