/**
 * Handles the location and speed of the elevator car
 */
#define _SUPPRESS_PLIB_WARNING 1
#define _DISABLE_OPENADC10_CONFIGPORT_WARNING 1

#include <plib.h>
#include <xc.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <FreeRTOS.h>
#include <timers.h>
#include <queue.h>
#include "physics.h"
#include "doordrv.h"

// Size of buffer of characters that get sent to the UART TX
#define BUFFER_SIZE 50

// Number of stops this elevator makes
#define NUM_STOPS 3

// Global variables
static volatile float cur_loc;
static volatile struct FloorRequest *dest;
static volatile float cur_speed, max_speed, accel;
static volatile bool going_up, accel_up;
static volatile bool emerg_stop_enabled;
static const TickType_t moveDelay = 500 / portTICK_PERIOD_MS;
static const TickType_t noDestPolling = 100 / portTICK_PERIOD_MS;

struct FloorRequest requests[] = {
    { false, UP, 0.0f, "GD" },
    { false, UP, 500.0f, "P1" },
    { false, DOWN, 510.0f, "P2" },
    { false, DOWN, 0.0f, "ES" }
};

// Constant strings used to send UART messages
static const char stopped[] = "Stopped";
static const char moving[] = "Moving";

/** Getters and Setters **/
bool GetIsMoving()
{
    return !((cur_loc == dest->feet) && (cur_speed == 0.0f));
}

struct FloorRequest GetRequest(int requestNum)
{
    return requests[requestNum];
}

void SetRequest(int requestNum, enum DIR dir)
{
    if(requests[requestNum].isRequested && 
      ((requests[requestNum].dir == UP && dir == DOWN) || 
      (requests[requestNum].dir == DOWN && dir == UP)))
        requests[requestNum].dir = EITHER;
    else
        requests[requestNum].dir = dir;
    
    requests[requestNum].isRequested = true;
}

bool GetGoingUp(void)
{
    return going_up;
}

float GetCurrentSpeed(void)
{
    return cur_speed;
}

void SetMaxSpeed(float speed)
{
    max_speed = speed;
}

void SetAccel(float new_accel)
{
    accel = new_accel;
}

void SetEmergStopEnable()
{
    emerg_stop_enabled = true;
}

/**
 * Move the elevator car (update location and speed)
 */
static void MoveCar(xPhysicsTaskParameter_t *taskParam)
{
    char buffer[BUFFER_SIZE];
    float decel_speed;
    
    accel_up = going_up;
    
    while(cur_loc != dest->feet)
    {
        vTaskDelay(moveDelay);  // Wait half a second

        // Start slowing down if we're in an emergency stop
        if(emerg_stop_enabled)
        {
            if(going_up)
            {
                dest = &(requests[3]);
                requests[3].feet = cur_loc + (cur_speed * cur_speed) / (2.0f * accel);
            }
            else
            {
                dest = &(requests[0]);
                accel_up = going_up;
            }
        }
        
        // Update location
        if(going_up)
            cur_loc += cur_speed / 2;
        else
            cur_loc -= cur_speed / 2;

        // Update speed
        if(going_up == accel_up)
            cur_speed += accel / 2;
        else
            cur_speed -= accel / 2;

        if(cur_speed > max_speed)
            cur_speed = max_speed;

        // If we're at max speed, then don't consider acceleration into the location (because it would be zero)
        if(cur_speed != max_speed)
        {
            if(accel_up)
                cur_loc += (0.125f * accel);
            else
                cur_loc += (-0.125f * accel);
        }

        // Calculate speed at which we should have started decelerating
        if(going_up)
            decel_speed = sqrtf(2.0f * accel * (dest->feet - cur_loc));
        else
            decel_speed = sqrtf(2.0f * accel * (cur_loc - dest->feet));

        // Correct for overshoot
        if(cur_speed >= decel_speed)
        {
            // Reverse the direction of acceleration if we haven't already
            if(accel_up == going_up)
                accel_up = !accel_up;

            cur_speed = decel_speed;
        }

        // If our speed is zero or below, then we've reached our destination
        if(cur_speed <= 0.0f || (!going_up && cur_loc <= dest->feet) || (going_up && cur_loc >= dest->feet))
        {
            cur_speed = 0.0f;
            cur_loc = dest->feet;
        }

        // Print out the current speed and destination
        snprintf(buffer, BUFFER_SIZE, "%.2f Feet :: %.2f ft/s\r\n", cur_loc, cur_speed);
        xQueueSendToBack(taskParam->tx_queue, (void*)buffer, 0);
    }
}

/**
 * Update where the elevator is moving to
 * 
 * @param taskParam The task's parameter struct
 * 
 * @return True if the destination was updated, false otherwise
 */
bool UpdateDestination(xPhysicsTaskParameter_t *taskParam)
{
    bool updated = false;
    
    // Update the direction
    if(cur_loc == requests[0].feet)
        going_up = true;
    else if(cur_loc == requests[1].feet && requests[1].dir == UP)
        going_up = true;
    else if(cur_loc == requests[1].feet && requests[1].dir == DOWN)
        going_up = false;
    else if(cur_loc == requests[2].feet)
        going_up = false;
    
    if(emerg_stop_enabled)
    {
        dest = &(requests[0]);
        dest->isRequested = false;
        updated = true;
        going_up = false;
    }
    else
    {
        if(going_up)
        {
            if(cur_loc == requests[0].feet)
            {
                if(requests[0].isRequested) { dest = &(requests[0]); dest->isRequested = false; updated = true; }
                else if(requests[1].isRequested && requests[1].dir == UP) { dest = &(requests[1]); dest->isRequested = false; updated = true; }
                else if(requests[1].isRequested && requests[1].dir == EITHER) { dest = &(requests[1]); requests[1].dir = DOWN; updated = true; }
                else if(requests[2].isRequested) { dest = &(requests[2]); dest->isRequested = false; updated = true; }
                else if(requests[1].isRequested && requests[1].dir == DOWN) { dest = &(requests[1]); dest->isRequested = false; updated = true; }
            }
            else if(cur_loc == requests[1].feet)
            {
                if(requests[1].isRequested && requests[1].dir == UP) { dest = &(requests[1]); dest->isRequested = false; updated = true; }
                else if(requests[1].isRequested && requests[1].dir == EITHER) { dest = &(requests[1]); requests[1].dir = DOWN; updated = true; }
                else if(requests[2].isRequested) { dest = &(requests[2]); dest->isRequested = false; updated = true; }
                else if(requests[1].isRequested && requests[1].dir == DOWN) { dest = &(requests[1]); dest->isRequested = false; updated = true; going_up = false; }
                else if(requests[0].isRequested) { dest = &(requests[0]); dest->isRequested = false; updated = true; going_up = false; }
            }
        }
        else
        {
            if(cur_loc == requests[1].feet)
            {
                if(requests[1].isRequested && requests[1].dir == DOWN) { dest = &(requests[1]); dest->isRequested = false; updated = true; }
                else if(requests[1].isRequested && requests[1].dir == EITHER) { dest = &(requests[1]); requests[1].dir = UP; updated = true; }
                else if(requests[0].isRequested) { dest = &(requests[0]); dest->isRequested = false; updated = true; }
                else if(requests[1].isRequested && requests[1].dir == UP) { dest = &(requests[1]); dest->isRequested = false; updated = true; going_up = true; }
                else if(requests[2].isRequested) { dest = &(requests[2]); dest->isRequested = false; updated = true; going_up = true; }
            }
            else if(cur_loc == requests[2].feet)
            {
                if(requests[2].isRequested) { dest = &(requests[2]); dest->isRequested = false; updated = true; }
                else if(requests[1].isRequested && requests[1].dir == DOWN) { dest = &(requests[1]); dest->isRequested = false; updated = true; }
                else if(requests[1].isRequested && requests[1].dir == EITHER) { dest = &(requests[1]); requests[1].dir = UP; updated = true; }
                else if(requests[0].isRequested) { dest = &(requests[0]); dest->isRequested = false; updated = true; }
                else if(requests[1].isRequested && requests[1].dir == UP) { dest = &(requests[1]); dest->isRequested = false; updated = true; }
            }
        }
    }
    
    // Update UP/DN Leds
    if(going_up)
    {
        mPORTBSetBits(BIT_5);
        mPORTBClearBits(BIT_4);
    }
    else
    {
        mPORTBSetBits(BIT_4);
        mPORTBClearBits(BIT_5);
    }
    
    return updated;
}

// Handle all of the physics calculations
void taskPhysics(void *pvParameters)
{
    char buffer[BUFFER_SIZE];
    enum DOOR_MSG msg;
    xPhysicsTaskParameter_t *taskParam;
    taskParam = (xPhysicsTaskParameter_t *)pvParameters;
    
    // Set defaults
    dest = &(requests[0]);
    max_speed = 50.0f;
    cur_loc = 0.0f;
    cur_speed = 0.0f;
    accel = 10.0f;
    going_up = true;
    accel_up = going_up;
    emerg_stop_enabled = false;
    
    while(1)
    {
        // If there's no destination, then wait
        while(!UpdateDestination(taskParam))
        {
            vTaskDelay(noDestPolling);
            
            // If somebody opened the door, wait for it to close
            if(!GetDoorClosed())
                xQueueReceive(taskParam->door_tx_queue, (void*)&msg, portMAX_DELAY);
        }
        
        // If we're moving, say so
        if(cur_loc != dest->feet)
        {
            snprintf(buffer, BUFFER_SIZE, "Floor %s %s\r\n", dest->acronym, moving);
            xQueueSendToBack(taskParam->tx_queue, (void*)buffer, 0);
        }
        
        MoveCar(taskParam);
        
        // The elevator has arrived at its destination
        snprintf(buffer, BUFFER_SIZE, "Floor %s %s\r\n", dest->acronym, stopped);
        xQueueSendToBack(taskParam->tx_queue, (void*)buffer, 0);
        
        // Handle door animation
        if(emerg_stop_enabled && (cur_loc == requests[0].feet))
        {
            msg = STAY_OPEN;
            xQueueOverwrite(taskParam->door_rx_queue, (void*)&msg);
            emerg_stop_enabled = false;
            
            // Wait for door to close
            xQueueReceive(taskParam->door_tx_queue, (void*)&msg, portMAX_DELAY);
        }
        else if(!emerg_stop_enabled)
        {
            msg = OPEN_CLOSE_SEQ;
            xQueueOverwrite(taskParam->door_rx_queue, (void*)&msg);
            
            // Wait for door to close
            xQueueReceive(taskParam->door_tx_queue, (void*)&msg, portMAX_DELAY);
        }
    }
}