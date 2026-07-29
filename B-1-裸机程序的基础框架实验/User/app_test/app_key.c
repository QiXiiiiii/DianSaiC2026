/**
  ******************************************************************************
  * @file    app_key.c
  * @author  embedfire
  * @brief   
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
  */

#include "app_key.h"

static Button btn1;
static Button btn2;

static uint8_t key_event;

void btn1_single_click_handler(Button* btn)
{
    (void)btn; 
    printf("🔘 Button 1: Single Click\n");
    
    key_event = 1;
}

void btn1_double_click_handler(Button* btn)
{
    (void)btn;  
    printf("🔘🔘 Button 1: Double Click\n");
    
    key_event = 2;
}

void btn2_single_click_handler(Button* btn)
{
    (void)btn;  
    printf("🔵 Button 2: Single Click\n");
    
    key_event = 3;
}

void btn2_double_click_handler(Button* btn)
{
    (void)btn; 
    printf("🔵🔵 Button 2: Double Click\n");
    
    key_event = 4;
}

uint8_t read_button_gpio(uint8_t button_id)
{
    switch (button_id) {
        case 1:
            return HAL_GPIO_ReadPin(KEY1_GPIO_PORT,KEY1_PIN);
        case 2:
            return HAL_GPIO_ReadPin(KEY2_GPIO_PORT,KEY2_PIN);
        default:
            return 0;
    }
}

void buttons_inti(void)
{
    button_init(&btn1, read_button_gpio, 1, 1);
    button_attach(&btn1, BTN_SINGLE_CLICK, btn1_single_click_handler);
    button_attach(&btn1, BTN_DOUBLE_CLICK, btn1_double_click_handler);    
    
    button_init(&btn2, read_button_gpio, 1, 2);   
    button_attach(&btn2, BTN_SINGLE_CLICK, btn2_single_click_handler);
    button_attach(&btn2, BTN_DOUBLE_CLICK, btn2_double_click_handler);    
    
    button_start(&btn1);
    button_start(&btn2);      
}


uint8_t get_key_event(void)
{
    uint8_t temp = key_event;    
    key_event = 0;    
    return temp;    
}

/*********************************************END OF FILE**********************/
