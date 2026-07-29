#ifndef __APP_KEY_H
#define	__APP_KEY_H

#include <stdio.h>
#include "multi_button.h"
#include "key/bsp_key.h"

void buttons_inti(void);

uint8_t get_key_event(void);
    

#endif /* __APP_KEY_H */
