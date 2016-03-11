/**
 * Handles polling the buttons for presses and responding to the button presses.
 * 
 * Five buttons:
 * SW1: P2 button inside the car
 * SW2: P1 button inside the car
 * SW3: GD button inside the car
 * SW4: Open door button inside the car
 * SW5: Close door button inside the car
 */
#define _SUPPRESS_PLIB_WARNING 1
#define _DISABLE_OPENADC10_CONFIGPORT_WARNING 1

#include <plib.h>
#include <xc.h>
#include <stdbool.h>
#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>
#include <queue.h>
#include "uartdrv.h"
#include "physics.h"
#include "btndrv.h"

// Macros for button GPIO lines
#define SW1 BIT_6
#define SW2 BIT_7
#define SW3 BIT_13
#define SW4 BIT_1
#define SW5 BIT_2

// Delay variables
static const TickType_t swDelay = 15 / portTICK_PERIOD_MS;
static const TickType_t pollDelay = 100 / portTICK_PERIOD_MS;

/**
 * Checks to see if a button is pressed, and if so, debounces it
 * 
 * @param swNum The switch on PORT D to check and debounce
 */ 
static bool CheckAndDebounceD(uint32_t swNum)
{
    bool sw_pressed = false;
    
    if(!mPORTDReadBits(swNum))
    {
        vTaskDelay(swDelay);
        if(!mPORTDReadBits(swNum))
            sw_pressed = true;
    }
    
    return sw_pressed;
}

/**
 * Checks to see if a button is pressed, and if so, debounces it
 * 
 * @param swNum The switch on PORT C to check and debounce
 */ 
static bool CheckAndDebounceC(uint32_t swNum)
{
    bool sw_pressed = false;
    
    if(!mPORTCReadBits(swNum))
    {
        vTaskDelay(swDelay);
        if(!mPORTCReadBits(swNum))
            sw_pressed = true;
    }
    
    return sw_pressed;
}

void SendToFloor(int floor)
{
    if(GetGoingUp())
        SetRequest(floor, UP);
    else
        SetRequest(floor, DOWN);
}

// Handle button presses and debouncing
void taskButtons(void *pvParameters)
{
    char buffer[TX_SIZE];
    xBtnTaskParameter_t *taskParam;
    taskParam = (xBtnTaskParameter_t *)pvParameters;
    
    while(1)
    {
        // P2 button inside car
        if(CheckAndDebounceD(SW1))
        {
            SendToFloor(2);
            snprintf(buffer, TX_SIZE, "Floor Requested\r\n");
            xQueueSendToBack(taskParam->tx_queue, (void*)buffer, 0);
        }

        // P1 button inside car
        if(CheckAndDebounceD(SW2))
        {
            SendToFloor(1);
            snprintf(buffer, TX_SIZE, "Floor Requested\r\n");
            xQueueSendToBack(taskParam->tx_queue, (void*)buffer, 0);
        }

        // GD button inside car
        if(CheckAndDebounceD(SW3))
        {
            SendToFloor(0);
            snprintf(buffer, TX_SIZE, "Floor Requested\r\n");
            xQueueSendToBack(taskParam->tx_queue, (void*)buffer, 0);
        }
        
        // Open door inside car
        if(CheckAndDebounceC(SW4))
        {
            enum DOOR_MSG msg;
            if(!GetIsMoving())
            {
                sprintf(buffer, "Door Opening\r\n");
                msg = OPEN_CLOSE_SEQ;
                xQueueOverwrite(taskParam->door_rx_queue, (void*)&msg);
            }
            else
                sprintf(buffer, "Can't open door while car is moving\r\n");
            
            xQueueSendToBack(taskParam->tx_queue, (void*)buffer, 0);
        }
        
        // Close door inside car
        if(CheckAndDebounceC(SW5))
        {
            enum DOOR_MSG msg;
    
            if(!GetIsMoving())
            {
                sprintf(buffer, "Door Closing\r\n");
                msg = CLOSE;
                xQueueOverwrite(taskParam->door_rx_queue, (void*)&msg);
                xQueueSendToBack(taskParam->tx_queue, (void*)buffer, 0);
            }
        }
        
        vTaskDelay(pollDelay);
    }
}

