/**
  ******************************************************************************
  * @file    data_test.c
  * @author  embedfire
  * @brief   
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
  */


#include "data_test.h"

static uint32_t get_rand_seed(void)
{
   uint32_t temp;
   return (uint32_t)&temp;      
}

static void rand_init(void)
{
    static uint8_t initialized  = 0;
    
    if (!initialized) {
        srand(get_rand_seed());
        initialized = 1;
    }
}
          


uint8_t get_rand_byte(void)
{
    rand_init();
    
    return rand() % 256;  
}


void fill_rand_byte_buffer(uint8_t* buffer, uint16_t size)
{
    rand_init();
    
    for(int i = 0; i < size; i++)
    {
        *buffer = rand() % 256;
         buffer++;      
    } 
}


void fill_sequential_byte_buffer(uint8_t* buffer, uint16_t size)
{ 
    for(int i = 0; i < size; i++)
    {
        *buffer = i % 256;
         buffer++;      
    } 
}


void printf_byte_buffer(uint8_t* buffer, uint16_t size)
{       
    for(int i = 0; i < size; i++)
    {
        if(i%8 == 0)
        {
            printf("\r\n");  
            printf("%04X:    ", i);  
        }       
        printf("%02X  ", *buffer);
        
        buffer++;      
    }  
    
    printf("\r\n");    
}


void fill_sequential_memory(uint32_t base, uint32_t* begin, uint32_t* end)
{   
    if(end < begin)
    {
        return;
    }
    
    uint16_t interval = end - begin;
    
    for(int i = 0; i < interval; i++)
    {
        *begin = base + i;
         begin++;      
    }       
}


void printf_memory(uint32_t* begin, uint32_t* end)
{ 
    if(end < begin)
    {
        return;
    }
    
    uint16_t interval = end - begin;
       
    for(int i = 0; i < interval; i++)
    {
        if(i%2 == 0)
        {
            printf("\r\n");  
            printf("%p:    ", begin);  
        }       
        printf("%08X  ", *begin);
        
        begin++;      
    }      
     
    printf("\r\n");
}



uint8_t compare_bufferrs(uint8_t* buffer1,uint8_t* buffer2, uint16_t size)
{
    uint8_t res = 0;
        
    for(int i = 0; i < size; i++)
    {
        if(*buffer1 != *buffer2)
        {
            res = 1;
            printf("Compare error in %d,  buffer1: %x , buffer2: %x \r\n",i,*buffer1,*buffer2);
        }    
    } 
    
    return res;
}



/*********************************************END OF FILE**********************/
