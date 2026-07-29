#ifndef __BSP_DWT_H
#define __BSP_DWT_H

#include "stdint.h"
#include "stm32f1xx.h"

/*
 在Cortex-M里面有一个外设叫DWT(Data Watchpoint and Trace)，该外设有一个32位的寄存器叫CYCCNT，它是一个向上的计数器，记录的是内核时钟运行的个数，
 
 假设内核频率为72M，DWT计数一次的时间约为1/72M=14ns，最长能记录的时间约为59s(2^32 / 72000000)，
 如果内核频率为480M，最长记录时间约为8.9s。
 
 当CYCCNT溢出之后，会清0重新开始向上计数。
 
 使能CYCCNT计数的操作步骤：
 1、先使能DWT外设准备，这个由另外内核调试寄存器DEMCR的位24控制，写1使能
 2、使能CYCCNT寄存器之前，先清0
 3、使能CYCCNT计数器，这个由DWT_CTRL的位0控制，写1使能
*/

/* DWT相关寄存器定义,实际上在CMSIS头文件内有相关定义，这里单独定义方便使用*/
#define  BSP_DEMCR                   *(uint32_t *)(0xE000EDFC)
#define  BSP_DWT_CTRL                *(uint32_t *)(0xE0001000)
#define  BSP_DWT_CYCCNT              *(uint32_t *)(0xE0001004)

#define  BSP_DEMCR_TRCENA            (1<<24)
#define  BSP_DWT_CTRL_CYCCNTENA      (1<<0)


void     DWT_Init(void);
uint32_t DWT_GetTick(void);
uint32_t DWT_TickToMicrosecond(uint32_t tick);
void     DWT_DelayUs(uint32_t us);
void     DWT_DelayMs(uint32_t ms);

#endif /* __BSP_DWT_H  */
