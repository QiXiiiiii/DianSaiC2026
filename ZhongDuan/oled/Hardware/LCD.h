#ifndef __LCD_H
#define __LCD_H

#include "main.h"

#define LCD_COLOR_BLACK       0x0000U
#define LCD_COLOR_WHITE       0xFFFFU
#define LCD_COLOR_RED         0xF800U
#define LCD_COLOR_GREEN       0x07E0U
#define LCD_COLOR_BLUE        0x001FU
#define LCD_COLOR_YELLOW      0xFFE0U
#define LCD_COLOR_CYAN        0x07FFU
#define LCD_COLOR_NAVY        0x000FU
#define LCD_COLOR_DARKGREY    0x4208U

uint16_t LCD_Init(void);
void LCD_Clear(uint16_t color);
void LCD_ShowLine(uint8_t line, const char *text,
                  uint16_t foreground, uint16_t background);
void LCD_ShowUtf8Line(uint8_t line, const char *text,
                      uint16_t foreground, uint16_t background);

#endif
