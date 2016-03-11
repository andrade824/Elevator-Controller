/**
 * Handles opening and closing the door on request.
 * 
 * Uses a queue to send and receive messages. This is how the other modules
 * tell the door to open and close (or stay opened in the case of an emergency
 * stop).
 */
#include <stdint.h>
#include <stdbool.h>
#include <FreeRTOS.h>
#include <timers.h>
#include <queue.h>
#include "doordrv.h"
#include "leddrv.h"

// Delays between states
static const TickType_t ledDelay = 1000 / portTICK_PERIOD_MS;
static const TickType_t pauseDelay = 5000 / portTICK_PERIOD_MS;

// Global variables
static bool opening;
static uint8_t cur_state;
static uint8_t next_state;

bool GetDoorClosed()
{
    return (cur_state == 0 && !opening);
}

/**
 * Handle opening and closing the door
 * 
 * @param pvParameters The task's parameters
 */
void taskDoor(void *pvParameters)
{
    enum DOOR_MSG msg;
    bool stay_opened = false;
    bool animation_done = false;
    xDoorTaskParameter_t *taskParam;
    taskParam = (xDoorTaskParameter_t *)pvParameters;
    
    opening = false;
    cur_state = 0;
    next_state = 0;
    
    // Show doors closed by default
    setLED(LED1, 1);
    setLED(LED2, 1);
    setLED(LED3, 1);
    
    while(1)
    {
        // Block until a door open message appears
        do
        {
            xQueueReceive(taskParam->door_rx_queue, &msg, portMAX_DELAY);

            switch(msg)
            {
                case OPEN_CLOSE_SEQ: opening = true; break;
                case STAY_OPEN: opening = true; stay_opened = true; break;
            }
        } while(!opening);
           
        // Perform door animation
        while(!animation_done)
        {
            // Check for any messages
            if(xQueueReceive(taskParam->door_rx_queue, &msg, 0) == pdTRUE)
            {
                if(msg == CLOSE)
                {
                    // Only close the doors after they've been opened for emergency stop
                    if(!stay_opened || (stay_opened && cur_state == 3))
                        opening = false;
                }
                else if(msg == OPEN_CLOSE_SEQ)
                {
                    if(!opening && cur_state >= 1 && cur_state <= 3 && !stay_opened)
                        next_state += 2;
                    
                    opening = true;
                }
            }
            
            cur_state = next_state;
            
            // State machine for how "open" the door is
            switch(cur_state)
            {
                case 0:
                    setLED(LED1, 1);
                    setLED(LED2, 1);
                    setLED(LED3, 1);

                    vTaskDelay(ledDelay);
                    
                    if(opening)
                        next_state = 1;
                    else
                        animation_done = true;

                    break;

                case 1:
                    setLED(LED1, 1);
                    setLED(LED2, 1);
                    setLED(LED3, 0);

                    vTaskDelay(ledDelay);
                    
                    if(opening)
                        next_state = 2;
                    else
                        next_state = 0;
                    
                    break;

                case 2:
                    setLED(LED1, 1);
                    setLED(LED2, 0);
                    setLED(LED3, 0);

                    vTaskDelay(ledDelay);
                    
                    if(opening)
                        next_state = 3;
                    else
                        next_state = 1;
                    
                    break;

                case 3:
                    setLED(LED1, 0);
                    setLED(LED2, 0);
                    setLED(LED3, 0);
                    
                    if(opening)
                    {
                        if(stay_opened)
                            vTaskDelay(ledDelay);
                        else
                            next_state = 4;
                    }
                    else
                        next_state = 2;
                    
                    break;
                    
                case 4:
                    vTaskDelay(pauseDelay);
                    next_state = 3;
                    opening = false;
                    
                    break;

                default: next_state = 0; break;
            }
        }
        
        // Send the door closed message
        msg = CLOSED;
        xQueueOverwrite(taskParam->door_tx_queue, (void*)&msg);
                
        opening = false;
        stay_opened = false;
        animation_done = false;
        cur_state = next_state = 0;
    }
}