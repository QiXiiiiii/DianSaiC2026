#ifndef __BSP_KEY_H
#define	__BSP_KEY_H

#include "stm32f1xx.h"
#include "main.h"


#define USE_KEU_EXTI_IRQ         0


//引脚定义
/*******************************************************/

#define KEY1_PIN                  GPIO_PIN_0                 
#define KEY1_GPIO_PORT            GPIOA                      


#define KEY2_PIN                  GPIO_PIN_13                 
#define KEY2_GPIO_PORT            GPIOC                      


/*******************************************************/

 /** 
	* 按键按下为高电平，  定义 KEY_DOWN_LEVIEL 1
	* 若按键按下为低电平，定义 KEY_UP_LEVIEL 0
	*/ 
#define KEY_DOWN_LEVIEL	1
#define KEY_UP_LEVIEL 0

void Key_GPIO_Config(void);

uint8_t Key_Scan(GPIO_TypeDef* GPIOx,uint16_t GPIO_Pin);

#endif /* __BSP_KEY_H */

