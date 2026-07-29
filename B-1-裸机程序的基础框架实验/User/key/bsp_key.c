/**
  ******************************************************************************
  * @file    bsp_key.c
  * @author  embedfire
  * @brief   
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
  */

#include "bsp_key.h" 
#include "bsp_led.h"
#include "bsp_beep.h" 

/**
  * @brief  配置按键用到的I/O口
  * @param  无
  * @retval 无
  */
void Key_GPIO_Config(void)
{
    /*定义一个GPIO_InitTypeDef类型的结构体*/
    GPIO_InitTypeDef GPIO_InitStructure;
    
    /*开启按键GPIO口的时钟*/
    __GPIOA_CLK_ENABLE();
    __GPIOC_CLK_ENABLE();
         
#if (USE_KEU_EXTI_IRQ == 1)  
              
     /*选择按键的引脚*/	
    GPIO_InitStructure.Pin = KEY1_PIN;     
    /*设置引脚为输入模式*/
    GPIO_InitStructure.Mode = GPIO_MODE_IT_RISING; 
    /*设置引脚不上拉也不下拉*/
    GPIO_InitStructure.Pull = GPIO_NOPULL;
    /*使用上面的结构体初始化按键*/
    HAL_GPIO_Init(KEY1_GPIO_PORT, &GPIO_InitStructure);
    /*选择按键的引脚*/	 
    GPIO_InitStructure.Pin = KEY2_PIN; 
    /*使用上面的结构体初始化按键*/
    HAL_GPIO_Init(KEY2_GPIO_PORT, &GPIO_InitStructure);       
                   
    /* 配置 EXTI 中断源 到key1 引脚、配置中断优先级*/
    HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
    /* 使能中断 */
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);    
        
    /* 配置 EXTI 中断源 到key2 引脚、配置中断优先级*/
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
    /* 使能中断 */
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);  
  
#else 

    /*选择按键的引脚*/	
    GPIO_InitStructure.Pin = KEY1_PIN;     
    /*设置引脚为输入模式*/
    GPIO_InitStructure.Mode = GPIO_MODE_INPUT; 
    /*设置引脚不上拉也不下拉*/
    GPIO_InitStructure.Pull = GPIO_NOPULL;
    /*使用上面的结构体初始化按键*/
    HAL_GPIO_Init(KEY1_GPIO_PORT, &GPIO_InitStructure);
    /*选择按键的引脚*/	 
    GPIO_InitStructure.Pin = KEY2_PIN; 
    /*使用上面的结构体初始化按键*/
    HAL_GPIO_Init(KEY2_GPIO_PORT, &GPIO_InitStructure); 
    
#endif       
}

/**
  * @brief   阻塞等待一次按键按下释放的过程
  * @param   具体的端口和端口位
  *		@arg GPIOx: x可以是（A...G） 
  *		@arg GPIO_PIN 可以是GPIO_PIN_x（x可以是1...16）
  * @retval  按键的状态
  *		@arg 1:按键按下与释放一次
  *		@arg 0:按键没按下
  */
uint8_t Key_Scan(GPIO_TypeDef* GPIOx,uint16_t GPIO_Pin)
{			
	/*检测是否有按键按下 */
	if(HAL_GPIO_ReadPin(GPIOx,GPIO_Pin) == KEY_DOWN_LEVIEL )  
	{	 
		/*等待按键释放 */
		while(HAL_GPIO_ReadPin(GPIOx,GPIO_Pin) == KEY_DOWN_LEVIEL);   
		return 1;	 
	}
	else
		return 0;
}


/*---------写法一 中断过程由HAL库处理，用户写需要的回调函数------*/
void EXTI0_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(KEY1_PIN); 
}


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)  
{
    if(GPIO_Pin == KEY1_PIN)  
    {            
        BEEP_TOGGLE
    }
}
/*------------------------------------------------------------------*/




/*--------写法二 在中断函数中根据各种寄存器标志状态编写过程-----------*/
void EXTI15_10_IRQHandler(void)
{   
	if(__HAL_GPIO_EXTI_GET_IT(KEY2_PIN) != RESET) //确保是否产生了EXTI Line中断
	{			
		LED2_TOGGLE;
        
		__HAL_GPIO_EXTI_CLEAR_IT(KEY2_PIN);   //清除中断标志位  
	}           
}
/*------------------------------------*/




/*********************************************END OF FILE**********************/

