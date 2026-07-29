#ifndef __BSP_LED_H
#define	__BSP_LED_H

#include "stm32f1xx.h"

//引脚定义
/*******************************************************/

#define LED1_PIN                  GPIO_PIN_2               
#define LED1_GPIO_PORT            GPIOC                    

#define LED2_PIN                  GPIO_PIN_3              
#define LED2_GPIO_PORT            GPIOC                      

/************************************************************/


/* 定义控制IO的宏 */
#define LED1_TOGGLE		HAL_GPIO_TogglePin(LED1_GPIO_PORT,LED1_PIN);
#define LED1_OFF		HAL_GPIO_WritePin(LED1_GPIO_PORT,LED1_PIN,GPIO_PIN_SET);
#define LED1_ON			HAL_GPIO_WritePin(LED1_GPIO_PORT,LED1_PIN,GPIO_PIN_RESET);
                        
#define LED2_TOGGLE		HAL_GPIO_TogglePin(LED2_GPIO_PORT,LED2_PIN);
#define LED2_OFF		HAL_GPIO_WritePin(LED2_GPIO_PORT,LED2_PIN,GPIO_PIN_SET);
#define LED2_ON			HAL_GPIO_WritePin(LED2_GPIO_PORT,LED2_PIN,GPIO_PIN_RESET);


//(全部打开)
#define LED_ALLON	\
					LED1_ON;\
					LED2_ON

					
//(全部关闭)
#define LED_ALLOFF	\
					LED1_OFF;\
					LED2_OFF


//(全部翻转)
#define LED_ALLTOGGLE \
					LED1_TOGGLE;\
					LED2_TOGGLE
				

					

void LED_GPIO_Config(void);

#endif /* __BSP_LED_H */
