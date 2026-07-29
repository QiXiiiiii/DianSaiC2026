/**
  ******************************************************************************
  * @file    bsp_uart1.c
  * @author  embedfire
  * @brief   
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
  */


#include "bsp_uart1.h"

UART_HandleTypeDef Uart1Handle;

static uint8_t              u1_rx_buffer[U1_BUFFER_MAX_SIZE];
static volatile uint16_t    u1_rx_size;
static volatile uint8_t     u1_rx_flag;

 /**
  * @brief  DEBUG_USART配置,工作模式配置 115200 8-N-1
  * @param  无
  * @retval 无
  */  
void UART1_Config(void)
{ 
    __USART1_CLK_ENABLE();
    
    Uart1Handle.Instance          = USART1;

    Uart1Handle.Init.BaudRate     = UART1_BAUDRATE;
    Uart1Handle.Init.WordLength   = UART_WORDLENGTH_8B;
    Uart1Handle.Init.StopBits     = UART_STOPBITS_1;
    Uart1Handle.Init.Parity       = UART_PARITY_NONE;
    Uart1Handle.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    Uart1Handle.Init.Mode         = UART_MODE_TX_RX;

    HAL_UART_Init(&Uart1Handle);
   
       
    UART1_Pin_Init();
    
    /*单独使能串口接收断 */
    __HAL_UART_ENABLE_IT(&Uart1Handle,UART_IT_RXNE);  
    
    /*单独使能串口空闲中断 */
    __HAL_UART_ENABLE_IT(&Uart1Handle,UART_IT_IDLE);  
    
    
    /* 设置中断优先级，使能中断 */
    HAL_NVIC_SetPriority(USART1_IRQn ,1,0);	
    HAL_NVIC_EnableIRQ(USART1_IRQn );	
}


/**
  * @brief UART 引脚初始化 
  * @param huart: UART handle
  * @retval 无
  */
void UART1_Pin_Init(void)
{  
    GPIO_InitTypeDef  GPIO_InitStruct = {0};
      
        __GPIOA_CLK_ENABLE();
  
        /* 配置Tx引脚为复用功能  */
        GPIO_InitStruct.Pin = UART1_TX_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(UART1_TX_GPIO_PORT, &GPIO_InitStruct);

        /* 配置Rx引脚为复用功能 */
        GPIO_InitStruct.Pin = UART1_RX_PIN;
        GPIO_InitStruct.Mode=GPIO_MODE_AF_INPUT;
        HAL_GPIO_Init(UART1_RX_GPIO_PORT, &GPIO_InitStruct); 
}


/* 发送字符串函数 */
void u1_send_string(char *str)
{
    uint16_t i = strlen(str);    
    HAL_UART_Transmit(&Uart1Handle,(uint8_t *)str,i,1000);  
}

/* 发送字节数组函数 */
void u1_send_bytes(uint8_t *byte,uint32_t len)
{
    HAL_UART_Transmit(&Uart1Handle,byte,len,1000);
}

/* 获取接收缓冲区状态,返回1表示有一帧内容待处理 */ 
uint8_t u1_get_rx_flag(void)
{
    return u1_rx_flag;   
}

/* 获取接收缓冲区数据源 */ 
uint8_t* u1_get_rx_data(void)
{
    return (uint8_t*)u1_rx_buffer;  
}

/* 获取接收缓冲区数据个数 */ 
uint16_t u1_get_rx_size(void)
{
    return u1_rx_size;  
}

/* 处理完一次接收缓冲区后，重置状态 */ 
void  u1_clean_rx_data(void)
{
    memset(u1_rx_buffer,0,u1_rx_size);         
    u1_rx_size = 0;
    u1_rx_flag = 0;  
}


/* 自行在中断函数里面处理接收中断的写法 */
void USART1_IRQHandler(void)
{     
   /* 编写自定义处理：是否有自定义帧头帧尾部、是否使用回车作帧间隔、是否使用空闲中断作帧间隔、是否立刻在中断里面处理缓冲区内容等等，
      例程举例以空闲中断判断为一帧数据内容,缓冲区每次只保存一帧内容，不在中断中处理缓冲区数据内容,赋值标志让主循环等待处理
   */
        
    /* 判断是RXNE标志的中断 */
	if(__HAL_UART_GET_FLAG(&Uart1Handle, UART_FLAG_RXNE ) != RESET)
	{		
        /* 读取串口DR寄存器值 */
        uint8_t byte = (uint16_t)READ_REG(Uart1Handle.Instance->DR);

        /* 缓冲区的一帧内容如果未处理，不覆盖 */
        if(u1_rx_flag == 0 )
        {
            u1_rx_buffer[u1_rx_size] = byte;
            u1_rx_size++;  
            
            /* 至多接收的个数，保留一字节作为空终止位，方便缓冲区内容为字符串时的处理 */
            if(u1_rx_size == U1_BUFFER_MAX_SIZE - 1)
            {
                u1_rx_flag = 1;
            }                      
        }        

        /* 清除对应标志,保存了接收数据后再调用*/
        __HAL_UART_CLEAR_FLAG(&Uart1Handle, UART_FLAG_RXNE);            
	} 

          
    /* 判断是IDLE标志的中断 */
	if(__HAL_UART_GET_FLAG( &Uart1Handle, UART_FLAG_IDLE ) != RESET)
	{		
        /* 以空闲中断判断为一帧数据内容 */
        u1_rx_flag = 1;
         
        /* 接收末尾赋值空终止位，方便缓冲区内容为字符串时的处理 */        
        u1_rx_buffer[u1_rx_size] = 0;
        
        /* 根据手册，清除空闲中断标志是使用读取DR寄存器的操作进行 */
        uint8_t byte = (uint16_t)READ_REG(Uart1Handle.Instance->DR);
	}   
}

/*********************************************END OF FILE**********************/
