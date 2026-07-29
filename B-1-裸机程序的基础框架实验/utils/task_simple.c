#include "task_simple.h"

typedef struct
{
    task_info_t*     tasks;
    uint16_t         task_num;
    uint8_t          ticks_status;
}task_control_t;

static task_control_t task_control;


void tasks_static_init(task_info_t* tasks,uint16_t num)
{
    task_control.tasks = tasks;
    task_control.task_num = num;
    task_control.ticks_status = 0;     
}


void tasks_static_add(uint16_t index, task_process_t function, uint32_t cycle, task_status_t status)
{
    task_control.tasks[index].process = function;
    task_control.tasks[index].cycle = cycle;
    task_control.tasks[index].timer = cycle;
    task_control.tasks[index].status = status; 
}


void tasks_ticks_enable(void)
{
    task_control.ticks_status = 1; 
}


void tasks_ticks_disable(void)
{
    task_control.ticks_status = 0;  
}


void tasks_ticks(void)
{
    if(task_control.ticks_status == 1)
    {               
        for(int i = 0; i < task_control.task_num; i++)
        {
            if(task_control.tasks[i].timer > 0)
            {
                task_control.tasks[i].timer--;
                
                if(task_control.tasks[i].timer == 0 && task_control.tasks[i].status == TASK_WAIT)
                {
                    task_control.tasks[i].status = TASK_RUN;
                }
            }
        }
    }    
}


void tasks_process(void)
{ 
    for(int i = 0; i < task_control.task_num; i++)
    {
        if(task_control.tasks[i].status == TASK_RUN)
        {          
            task_control.tasks[i].process();
            
            task_control.tasks[i].timer = task_control.tasks[i].cycle;
            task_control.tasks[i].status = TASK_WAIT;
        }
    }	       
}


void task_set_cycle(uint16_t index, uint32_t cycle)
{
    task_control.tasks[index].cycle = cycle;
    task_control.tasks[index].timer = cycle; 
}


void task_set_status(uint16_t index, task_status_t status)
{
    task_control.tasks[index].status = status;       
}
   

void task_set_process(uint16_t index, task_process_t function)
{
    task_control.tasks[index].process = function;     
}

