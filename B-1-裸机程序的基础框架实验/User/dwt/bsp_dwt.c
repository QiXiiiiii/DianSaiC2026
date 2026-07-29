/**
  ******************************************************************************
  * @file    bsp_dwt.c
  * @author  embedfire
  * @brief   
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
  */
  

#include "bsp_dwt.h"

/* 用HAL库函数获取系统频率,可替换自定义实现*/
#define GET_SYSTEMCORECLOCK HAL_RCC_GetHCLKFreq()


/**
  * @brief  初始化DWT计数器
  * @param  无
  * @retval 无
  * @note   使用延时函数前，必须调用本函数
  */
void DWT_Init(void)
{
    /* 使能DWT外设 */
    BSP_DEMCR |= (uint32_t)BSP_DEMCR_TRCENA;
    
    /* DWT CYCCNT寄存器计数清0 */
    BSP_DWT_CYCCNT = (uint32_t)0U;
    
    /* 使能Cortex-M DWT CYCCNT寄存器 */
    BSP_DWT_CTRL  |=(uint32_t)BSP_DWT_CTRL_CYCCNTENA;
}


/**
  * @brief  读取当前时间戳
  * @param  无
  * @retval 当前时间戳，即DWT_CYCCNT寄存器的值
  */
uint32_t DWT_GetTick(void)
{ 
    return ((uint32_t)BSP_DWT_CYCCNT);
}

/**
  * @brief  节拍数转化时间间隔(微妙单位)
  * @param  tick :需要转换的节拍数
  * @retval 当前时间戳(微妙单位)
  */
uint32_t DWT_TickToMicrosecond(uint32_t tick)
{ 
    return (uint32_t)(1000000.0 / GET_SYSTEMCORECLOCK * tick);
}


/**
  * @brief  DWT计数实现较精确的延时
  * @param  time : 延迟长度，单位1 us
  * @retval 无
  * @note   无
  */
void DWT_DelayUs(uint32_t us)
{
    /* 将微秒转化为对应的时钟计数值*/
    uint32_t tick_duration= us * (GET_SYSTEMCORECLOCK / 1000000) ;
    
    /* 刚进入时的计数器值 */
    uint32_t tick_start = DWT_GetTick();         
    
    /* 阻塞等待直到间隔时间足够 */
    while(DWT_GetTick() - tick_start < tick_duration);
}

/**
  * @brief  DWT计数实现较精确的延时
  * @param  time : 延迟长度，单位1 ms
  * @retval 无
  */
void DWT_DelayMs(uint32_t ms)
{   
    DWT_DelayUs(1000*ms);
}

/*********************************************END OF FILE**********************/

