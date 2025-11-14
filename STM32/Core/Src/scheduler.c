/*
 * scheduler.c
 *
 *  Created on: Nov 13, 2025
 *      Author: THINH
 */
#include "scheduler.h"
#include <stddef.h>
#include "stm32f1xx.h"


/* Array of tasks */
static sTask SCH_tasks_G[SCH_MAX_TASKS];

/* Linked-list for delta timing: next index or NO_TASK */
#define NO_TASK_ID   0xFFFFFFFFU
static uint32_t Next[SCH_MAX_TASKS];

/* Index of head of delta-list */
static uint32_t head = NO_TASK_ID;


static inline uint32_t ms_to_ticks(uint32_t ms)
{
    if (TIME_CYCLE <= 0) return ms;
    return (ms + TIME_CYCLE - 1) / TIME_CYCLE;   // round up
}

/* Disable IRQ macros */
#define ENTER_CRITICAL() __disable_irq()
#define EXIT_CRITICAL()  __enable_irq()


/*Initialization*/

void SCH_Init(void)
{
    ENTER_CRITICAL();

    for (uint32_t i = 0; i < SCH_MAX_TASKS; i++) {
        SCH_tasks_G[i].pTask = NULL;
        SCH_tasks_G[i].Delay = 0;
        SCH_tasks_G[i].Period = 0;
        SCH_tasks_G[i].RunMe = 0;
        SCH_tasks_G[i].TaskID = i;

        Next[i] = NO_TASK_ID;
    }

    head = NO_TASK_ID;

    EXIT_CRITICAL();
}


/*Insert node*/

static void insert_task(uint32_t id, uint32_t delay)
{
    uint32_t prev = NO_TASK_ID;
    uint32_t cur = head;
    uint32_t remaining = delay;

    /* Walk until insert position */
    while (cur != NO_TASK_ID && remaining > SCH_tasks_G[cur].Delay) {
        remaining -= SCH_tasks_G[cur].Delay;
        prev = cur;
        cur = Next[cur];
    }

    /* Insert id */
    SCH_tasks_G[id].Delay = remaining;
    Next[id] = cur;

    if (cur != NO_TASK_ID) {
        SCH_tasks_G[cur].Delay -= remaining;
    }

    if (prev == NO_TASK_ID) {
        head = id;
    } else {
        Next[prev] = id;
    }
}


/*Delete task*/

static uint8_t remove_task(uint32_t id)
{
    uint32_t prev = NO_TASK_ID;
    uint32_t cur = head;

    while (cur != NO_TASK_ID && cur != id) {
        prev = cur;
        cur = Next[cur];
    }

    if (cur == NO_TASK_ID) return RETURN_ERROR;

    uint32_t next = Next[cur];

    /* Fix delta times */
    if (next != NO_TASK_ID) {
        SCH_tasks_G[next].Delay += SCH_tasks_G[cur].Delay;
    }

    /* Remove */
    if (prev == NO_TASK_ID)
        head = next;
    else
        Next[prev] = next;

    Next[id] = NO_TASK_ID;
    return RETURN_NORMAL;
}


/*Add Task*/

uint32_t SCH_Add_Task(void (*pFunction)(void), uint32_t DELAY_ms, uint32_t PERIOD_ms)
{
    if (pFunction == NULL) return SCH_MAX_TASKS;

    /* Find free slot */
    uint32_t id;
    for (id = 0; id < SCH_MAX_TASKS; id++) {
        if (SCH_tasks_G[id].pTask == NULL) break;
    }

    if (id == SCH_MAX_TASKS) return SCH_MAX_TASKS;

    uint32_t delayTicks  = ms_to_ticks(DELAY_ms);
    uint32_t periodTicks = ms_to_ticks(PERIOD_ms);

    if (DELAY_ms != 0 && delayTicks == 0) delayTicks = 1;
    if (PERIOD_ms != 0 && periodTicks == 0) periodTicks = 1;

    ENTER_CRITICAL();

    SCH_tasks_G[id].pTask  = pFunction;
    SCH_tasks_G[id].Delay  = 0;  // will be set by insert
    SCH_tasks_G[id].Period = periodTicks;
    SCH_tasks_G[id].RunMe  = 0;
    Next[id] = NO_TASK_ID;

    insert_task(id, delayTicks);

    EXIT_CRITICAL();

    return id;
}


/*Delete Task*/

uint8_t SCH_Delete_Task(uint32_t id)
{
    if (id >= SCH_MAX_TASKS) return RETURN_ERROR;

    ENTER_CRITICAL();

    if (SCH_tasks_G[id].pTask == NULL) {
        EXIT_CRITICAL();
        return RETURN_ERROR;
    }

    remove_task(id);

    SCH_tasks_G[id].pTask = NULL;
    SCH_tasks_G[id].Delay = 0;
    SCH_tasks_G[id].Period = 0;
    SCH_tasks_G[id].RunMe = 0;

    EXIT_CRITICAL();
    return RETURN_NORMAL;
}


/*ISR: SCH_Update*/

void SCH_Update(void)
{
    ENTER_CRITICAL();

    if (head == NO_TASK_ID) {
        EXIT_CRITICAL();
        return;
    }

    /* Decrement delay at head */
    if (SCH_tasks_G[head].Delay > 0)
        SCH_tasks_G[head].Delay--;

    /* Pop all ready tasks */
    while (head != NO_TASK_ID && SCH_tasks_G[head].Delay == 0) {
        uint32_t id = head;
        head = Next[id];
        Next[id] = NO_TASK_ID;
        SCH_tasks_G[id].RunMe++;
    }

    EXIT_CRITICAL();
}


/*Dispatcher*/

void SCH_Dispatch_Tasks(void)
{
    for (uint32_t i = 0; i < SCH_MAX_TASKS; i++) {
        if (SCH_tasks_G[i].pTask && SCH_tasks_G[i].RunMe > 0) {

            ENTER_CRITICAL();
            SCH_tasks_G[i].RunMe--;
            EXIT_CRITICAL();

            /* Execute task */
            (*SCH_tasks_G[i].pTask)();

            /* Re-schedule periodic tasks */
            if (SCH_tasks_G[i].Period > 0) {
                ENTER_CRITICAL();
                insert_task(i, SCH_tasks_G[i].Period);
                EXIT_CRITICAL();
            } else {
                /* One-shot: delete it */
                SCH_Delete_Task(i);
            }
        }
    }
}
