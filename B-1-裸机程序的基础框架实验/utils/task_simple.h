#ifndef __TASK_SIMPLE_H
#define	__TASK_SIMPLE_H

#include <stdint.h>

typedef void (*task_process_t)(void);


typedef enum
{
   TASK_STOP = 0, 
   TASK_RUN,
   TASK_WAIT,       
}task_status_t;


typedef struct
{
    task_process_t   process;
    task_status_t    status;
    uint32_t         cycle;
    uint32_t         timer;
}task_info_t;


void tasks_static_init(task_info_t* tasks,uint16_t num);
void tasks_static_add(uint16_t index, task_process_t function, uint32_t cycle, task_status_t status);

void tasks_ticks_enable(void);
void tasks_ticks_disable(void);

void tasks_ticks(void);
void tasks_process(void);

void task_set_cycle(uint16_t index, uint32_t cycle);
void task_set_status(uint16_t index, task_status_t status);
void task_set_process(uint16_t index, task_process_t function);

#endif /* __TASK_SIMPLE_H */
