#ifndef __PASSWORD_UI_H
#define __PASSWORD_UI_H

#include "main.h"

typedef enum {
    PASSWORD_UI_EVENT_NONE = 0,
    PASSWORD_UI_EVENT_TOGGLE_LOCK,
    PASSWORD_UI_EVENT_ZIGBEE
} PasswordUiEvent;

void PasswordUI_Init(uint8_t password_unlocked);
PasswordUiEvent PasswordUI_Task(uint8_t password_unlocked);
uint8_t PasswordUI_IsActive(void);
void PasswordUI_ShowMainButtons(uint8_t password_unlocked);

#endif
