/**
  ******************************************************************************
  * @file    main.c
  * @author  embedfire
  * @brief   
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
  */

#include <stdio.h>
#include <string.h>
#include "main.h"
#include "stm32f1xx.h"
#include "bsp_led.h"
#include "bsp_key.h"
#include "bsp_beep.h"  
#include "bsp_uart1.h"
#include "bsp_basic_tim6.h"
#include "bsp_dwt.h"
#include "task_simple.h"
#include "app_key.h"

static void SystemClock_Config(void);
void Delay( uint32_t nCount);


enum { 
    TEST1_TASK = 0,
    TEST2_TASK,
    TEST3_TASK,
    TEST4_TASK,
    TASK_NUM
};

task_info_t tasks[TASK_NUM];


void Test1_Process(void)
{		
    LED2_TOGGLE; 
}

void Test2_Process(void)
{       
    printf("Periodic printing...\r\n");      
}

void Test3_Process(void)
{  
    if(u1_get_rx_flag() == 1)
    {    
        uint8_t *data = u1_get_rx_data(); 
        uint16_t size = u1_get_rx_size();

        if(strstr((char*)data,"LED2_ON") != NULL)
        {
            LED2_ON;
        }

        if(strstr((char*)data,"LED2_OFF") != NULL)
        {
            LED2_OFF;
        }              
                    
        /* 回显接收内容 */ 
        u1_send_bytes(data,size); 

        /* 处理完记得调用 */ 
        u1_clean_rx_data();  
    }    
}

void Test4_Process(void)
{  
    uint8_t key_event = get_key_event();
    
    if(key_event != 0)
    {    
        printf("get_key_event: %d \r\n",key_event);  
    }    
}

/**
  * @brief  主函数
  * @param  
  * @retval 
  */
int main(void)
{
    /* 初始化系统中断组、系统定时器等 */
    HAL_Init();
    
     /* 配置系统时钟频率 */
    SystemClock_Config();
     
    /* 串口1初始化 */
    UART1_Config();
    
    /* KEY 端口初始化 */
    Key_GPIO_Config();   
        
    /* LED 端口初始化 */
    LED_GPIO_Config();  

    /* 初始化DWT */
    DWT_Init();
    
    /* 初始化一个定时器定时中断执行任务 */
    TIM6_Config();  
     
    /* 初始化按键库 */   
    buttons_inti();
        
    printf("欢迎使用野火开发板 \r\n");
    printf("请看readme中的实验操作 \r\n");
        
    tasks_static_init(tasks,TASK_NUM);  
    tasks_static_add(TEST1_TASK,Test1_Process,1000,TASK_WAIT);
    tasks_static_add(TEST2_TASK,Test2_Process,3000,TASK_WAIT);   
    tasks_static_add(TEST3_TASK,Test3_Process,10, TASK_WAIT);
    tasks_static_add(TEST4_TASK,Test4_Process,100, TASK_WAIT);    
    tasks_ticks_enable();
     
    while (1)
    {
        tasks_process();   
    }
}


/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow : 
  *            System Clock source            = PLL (HSE)
  *            SYSCLK(Hz)                     = 72000000
  *            HCLK(Hz)                       = 72000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 2
  *            APB2 Prescaler                 = 1
  *            HSE Frequency(Hz)              = 8000000
  *            HSE PREDIV1                    = 1
  *            PLLMUL                         = 9
  *            Flash Latency(WS)              = 2
  * @param  None
  * @retval None
  */
static void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef clkinitstruct = {0};
  RCC_OscInitTypeDef oscinitstruct = {0};
  
  /* Enable HSE Oscillator and activate PLL with HSE as source */
  oscinitstruct.OscillatorType  = RCC_OSCILLATORTYPE_HSE;
  oscinitstruct.HSEState        = RCC_HSE_ON;
  oscinitstruct.HSEPredivValue  = RCC_HSE_PREDIV_DIV1;
  oscinitstruct.PLL.PLLState    = RCC_PLL_ON;
  oscinitstruct.PLL.PLLSource   = RCC_PLLSOURCE_HSE;
  oscinitstruct.PLL.PLLMUL      = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&oscinitstruct)!= HAL_OK)
  {
    /* Initialization Error */
    while(1); 
  }

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 
     clocks dividers */
  clkinitstruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  clkinitstruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clkinitstruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clkinitstruct.APB2CLKDivider = RCC_HCLK_DIV1;
  clkinitstruct.APB1CLKDivider = RCC_HCLK_DIV2;  
  if (HAL_RCC_ClockConfig(&clkinitstruct, FLASH_LATENCY_2)!= HAL_OK)
  {
    /* Initialization Error */
    while(1); 
  }
}



/**
  * @brief  模拟不精确延时函数
  * @param  空指令数
  */
void Delay(uint32_t nCount)	
{
	for(uint32_t i = nCount; i != 0; i--)
    {
        __ASM("NOP"); //需要空指令占位，否则在高优化等级失效
    }
}


/* 勾选Use MicroLIB情况下，重定向c库函数printf到调试用串口*/
int fputc(int ch, FILE *f)
{
	HAL_UART_Transmit(&Uart1Handle, (uint8_t *)&ch, 1, 1000);		
	return (ch);
}

