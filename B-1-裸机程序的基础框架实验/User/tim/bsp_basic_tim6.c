/**
  ******************************************************************************
  * @file    bsp_.c
  * @author  embedfire
  * @brief   
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
  */

#include "multi_button.h"
#include "bsp_basic_tim6.h"


TIM_HandleTypeDef htim6;


void TIM6_Config(void)
{
    __HAL_RCC_TIM6_CLK_ENABLE(); 
    
    /* 配置定时器基础 */
    htim6.Instance = TIM6;
    htim6.Init.Prescaler = TIM6_DEF_PRESCALER;
    htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim6.Init.Period = TIM6_DEF_PERIOD;
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;//建议无特殊需求时保持使能，方便外部调整重载值
    HAL_TIM_Base_Init(&htim6);
    
    HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);   
    
    /* 启动定时器和使能中断 */
    HAL_TIM_Base_Start_IT(&htim6);	
}


 /* 每5ms执行一次按键库流程，此处直接手写简短过程比使用库处理回调函数更高效 */
void TIM6_DAC_IRQHandler(void)
{
    if(__HAL_TIM_GET_FLAG(&htim6, TIM_FLAG_UPDATE))
    {
        button_ticks();
              
        __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);       
    }
}

/*********************************************END OF FILE**********************/
