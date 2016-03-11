/**
 * Pulses a GPIO pin (in this case, RF8) 1Hz for every 10ft/s of movement
 */
#define _SUPPRESS_PLIB_WARNING 1
#define _DISABLE_OPENADC10_CONFIGPORT_WARNING 1

#include <plib.h>
#include <xc.h>
#include <math.h>
#include <FreeRTOS.h>
#include <task.h>
#include "physics.h"

// Handle toggling the motor
void taskMotor(void *pvParameters)
{
    float cur_speed;
    int hz = 1;
    
    while(1)
    {
        cur_speed = GetCurrentSpeed();
        
        if(cur_speed > 0.0f)
        {
            hz = (int)floorf(cur_speed / 10.0f);
            if(hz < 1)
                hz = 1;
            mPORTFToggleBits(BIT_8);
        }
        else
        {
            hz = 1;
            mPORTFClearBits(BIT_8);
        }
        
        vTaskDelay((1000 / hz) / portTICK_PERIOD_MS);
    }
}
