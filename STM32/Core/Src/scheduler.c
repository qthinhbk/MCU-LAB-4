/*
 * scheduler.c
 *
 *  Created on: Nov 13, 2025
 *      Author: THINH
 */
#include "scheduler.h"
#include <stddef.h>

/* Scheduler storage */
sTask SCH_tasks_G[SCH_MAX_TASKS];

/* Optional: global error code (not used heavily here) */
static uint8_t Error_code_G = 0;

/* Ensure TIME_CYCLE is sensible (set in main's MX_TIM2_Init) */
extern int TIME_CYCLE;

/* Initialize scheduler: clear task table */
void SCH_Init(void)
{
    for (uint32_t i = 0; i < SCH_MAX_TASKS; i++) {
        SCH_tasks_G[i].pTask = NULL;
        SCH_tasks_G[i].Delay = 0;
        SCH_tasks_G[i].Period = 0;
        SCH_tasks_G[i].RunMe = 0;
        SCH_tasks_G[i].TaskID = i;
    }

    /* Defensive: if TIME_CYCLE not set, default to 1 ms to avoid div/0 */
    if (TIME_CYCLE <= 0) {
        TIME_CYCLE = 1;
    }

    Error_code_G = 0;
}

/* Add a new task.
   DELAY_ms and PERIOD_ms are in milliseconds.
   Returns task index (0..SCH_MAX_TASKS-1) or SCH_MAX_TASKS on failure. */
uint32_t SCH_Add_Task(void (*pFunction)(void), uint32_t DELAY_ms, uint32_t PERIOD_ms)
{
    if (pFunction == NULL) {
        return SCH_MAX_TASKS;
    }

    /* find first free slot */
    uint32_t Index;
    for (Index = 0; Index < SCH_MAX_TASKS; Index++) {
        if (SCH_tasks_G[Index].pTask == NULL) break;
    }

    if (Index == SCH_MAX_TASKS) {
        /* No space */
        Error_code_G = 1; /* arbitrary error code */
        return SCH_MAX_TASKS;
    }

    /* Convert milliseconds to scheduler ticks (using TIME_CYCLE ms per tick) */
    uint32_t delayTicks  = (TIME_CYCLE > 0) ? (DELAY_ms  / (uint32_t)TIME_CYCLE)  : DELAY_ms;
    uint32_t periodTicks = (TIME_CYCLE > 0) ? (PERIOD_ms / (uint32_t)TIME_CYCLE) : PERIOD_ms;

    /* If DELAY_ms was non-zero but smaller than TIME_CYCLE, we still want it to fire on next tick:
       keep at least 1 tick for non-zero delays */
    if ((DELAY_ms != 0) && (delayTicks == 0)) delayTicks = 1;
    if ((PERIOD_ms != 0) && (periodTicks == 0)) periodTicks = 1;

    SCH_tasks_G[Index].pTask  = pFunction;
    SCH_tasks_G[Index].Delay  = delayTicks;
    SCH_tasks_G[Index].Period = periodTicks;
    SCH_tasks_G[Index].RunMe  = 0;
    SCH_tasks_G[Index].TaskID = Index;

    return Index;
}

/* Delete task by index.
   Returns RETURN_NORMAL (0) on success, RETURN_ERROR (1) on failure. */
uint8_t SCH_Delete_Task(uint32_t taskID)
{
    if (taskID >= SCH_MAX_TASKS) return RETURN_ERROR;

    if (SCH_tasks_G[taskID].pTask == NULL) {
        /* nothing to delete */
        return RETURN_ERROR;
    }

    SCH_tasks_G[taskID].pTask  = NULL;
    SCH_tasks_G[taskID].Delay  = 0;
    SCH_tasks_G[taskID].Period = 0;
    SCH_tasks_G[taskID].RunMe  = 0;
    /* TaskID field can remain equal to index */

    return RETURN_NORMAL;
}

/* This function should be called from a timer ISR every TIME_CYCLE ms.
   It MUST be short and deterministic. */
void SCH_Update(void)
{
    /* Defensive: if TIME_CYCLE invalid, nothing to do */
    if (TIME_CYCLE <= 0) return;

    for (uint32_t i = 0; i < SCH_MAX_TASKS; i++) {
        if (SCH_tasks_G[i].pTask != NULL) {
            if (SCH_tasks_G[i].Delay == 0) {
                /* Task due to run */
                SCH_tasks_G[i].RunMe += 1;
                /* If periodic, reload Delay; if one-shot (Period==0) leave Delay==0 */
                if (SCH_tasks_G[i].Period > 0) {
                    SCH_tasks_G[i].Delay = SCH_tasks_G[i].Period;
                }
            } else {
                SCH_tasks_G[i].Delay--;
            }
        }
    }
}

/* Dispatcher: call due tasks. Should run in main loop (non-ISR). */
void SCH_Dispatch_Tasks(void)
{
    for (uint32_t i = 0; i < SCH_MAX_TASKS; i++) {
        if (SCH_tasks_G[i].pTask != NULL) {
            if (SCH_tasks_G[i].RunMe > 0) {
                /* run the task */
                SCH_tasks_G[i].RunMe--;
                (*(SCH_tasks_G[i].pTask))();

                /* if one-shot then delete it */
                if (SCH_tasks_G[i].Period == 0) {
                    SCH_Delete_Task(i);
                }
            }
        }
    }
}
