#ifndef __TOUCH_H
#define __TOUCH_H

#include "main.h"

/* XPT2046 software-SPI wiring on the Wildfire LCD connector. */
#define TOUCH_CLK_PIN       GPIO_PIN_8
#define TOUCH_CS_PIN        GPIO_PIN_9
#define TOUCH_MOSI_PIN      GPIO_PIN_10
#define TOUCH_MISO_PIN      GPIO_PIN_11
#define TOUCH_IRQ_PIN       GPIO_PIN_12
#define TOUCH_PORT          GPIOC

void Touch_Init(void);
uint8_t Touch_IsPressed(void);
uint8_t Touch_Read(uint16_t *x, uint16_t *y);
void Touch_GetLastRaw(uint16_t *raw_x, uint16_t *raw_y);

#endif
