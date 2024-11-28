#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "pump_control.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "configuration.h"

unsigned short get_cur_min_of_day()
{
    time_t now;
    struct tm timeinfo;
    // Set timezone to Germany Berlin
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_hour * 60 + timeinfo.tm_min;
}

/* Dimensions of the buffer that the task being created will use as its stack.
   NOTE: This is the number of words the stack will hold, not the number of
   bytes. For example, if each stack item is 32-bits, and this is set to 100,
   then 400 bytes (100 * 32-bits) will be allocated. */
#define STACK_SIZE 200

/* Structure that will hold the TCB of the task being created. */

StaticTask_t xTaskBuffer;

/* Buffer that the task being created will use as its stack. Note this is
   an array of StackType_t variables. The size of StackType_t is dependent on
   the RTOS port. */
StackType_t xStack[STACK_SIZE];

/* Function that implements the task being created. */
void pump_control_task(void *pvParameters)
{
    /* The parameter value is expected to be 1 as 1 is passed in the

       pvParameters value in the call to xTaskCreateStatic(). */

    configASSERT((uint32_t)pvParameters == 1UL);

    const TickType_t xDelay = 1000 / portTICK_PERIOD_MS;
    unsigned short last_run = 0;
    bool pumping = false;
    time_t start_time;
    time_t now;

    for (;;)
    {
        if (pumping)
        {
            time(&now);
            double diff_t = difftime(now, start_time);
            const unsigned short pump_time_s = configuration.pump_cycles.pump_time_s;
            if (diff_t >= pump_time_s)
            {
                // disable pump
                pumping = false;
            }
        }
        else
        {
            unsigned short curr_min = get_cur_min_of_day();
            const unsigned short nr_pump_cycles = configuration.pump_cycles.nr_pump_cycles;
            const unsigned short times_minutes_per_day = configuration.pump_cycles.times_minutes_per_day;
            bool last_run_needs_reset = true;
            for (size_t i = 0; i < nr_pump_cycles; i++)
            {

                if (times_minutes_per_day[i] > last_run)
                {
                    // there exist a schedule time point in the future no need to reset
                    last_run_needs_reset = false;

                    if (curr_min >= times_minutes_per_day[i])
                    {
                        // start_running
                        // enable pump
                        pumping = true;
                        last_run = times_minutes_per_day[i];
                        time(&start_time);
                        break;
                    }
                }
            }
            if (last_run_needs_reset)
            {
                last_run = 0;
            }
        }

        vTaskDelay(xDelay);
    }
}

/* Function that creates a task. */

void create_pump_control_task()
{
    TaskHandle_t xHandle = NULL;

    /* Create the task without using any dynamic memory allocation. */
    xHandle = xTaskCreateStatic(
        pump_control_task,
        "PumpControl",        /* Text name for the task. */
        STACK_SIZE,           /* Number of indexes in the xStack array. */
        (void *)1,            /* Parameter passed into the task. */
        tskIDLE_PRIORITY + 1, /* Priority at which the task is created. */
        xStack,               /* Array to use as the task's stack. */
        &xTaskBuffer);        /* Variable to hold the task's data structure. */

    /* puxStackBuffer and pxTaskBuffer were not NULL, so the task will have

       been created, and xHandle will be the task's handle. Use the handle

       to suspend the task. */

    // [vTaskSuspend](/ Documentation / 02 - Kernel / 04 - API - references / 02 - Task - control / 06 - vTaskSuspend)(xHandle);
}