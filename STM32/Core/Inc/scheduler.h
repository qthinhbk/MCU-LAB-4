/*
 * scheduler.h
 *
 *  Created on: Nov 13, 2025
 *      Author: THINH
 */

#ifndef INC_SCHEDULER_H_
#define INC_SCHEDULER_H_

#include <stdint.h>

#define SCH_MAX_TASKS    40

/* Return codes */
#define RETURN_NORMAL    0
#define RETURN_ERROR     1

extern int TIME_CYCLE; /* in ms (set by MX_TIM2_Init) */

typedef struct {
    void (*pTask)(void);   /* Pointer to the task (NULL if empty) */
    uint32_t Delay;        /* Ticks until first run (in scheduler ticks) */
    uint32_t Period;       /* Periodic interval (in scheduler ticks). 0 = one-shot */
    uint8_t  RunMe;        /* Scheduler increments when task is due */
    uint32_t TaskID;       /* Index in task array */
} sTask;

/* API */
void SCH_Init(void);
uint32_t SCH_Add_Task(void (*pFunction)(void), uint32_t DELAY_ms, uint32_t PERIOD_ms);
uint8_t  SCH_Delete_Task(uint32_t taskID);
void SCH_Update(void);            /* To be called from timer ISR (every TIME_CYCLE ms) */
void SCH_Dispatch_Tasks(void);    /* To be called in main loop */

#endif /* INC_SCHEDULER_H_ */
