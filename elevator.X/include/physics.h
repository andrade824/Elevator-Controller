#ifndef PHYSICS_H
#define	PHYSICS_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <queue.h>
#include "doordrv.h"
    
typedef struct xPHYSICS_TASK_PARAMETER {
    QueueHandle_t tx_queue;
    QueueHandle_t door_rx_queue;    // Door receives messages on this queue
    QueueHandle_t door_tx_queue;    // Door transmits messages on this queue
} xPhysicsTaskParameter_t;

enum DIR { UP, DOWN, EITHER };

struct FloorRequest {
    bool isRequested;
    enum DIR dir;
    float feet;
    char acronym[3];
};

// Physics Task
void taskPhysics(void *pvParameters);

// Getters and Setters
bool GetIsMoving();
struct FloorRequest GetRequest(int requestNum);
void SetRequest(int requestNum, enum DIR dir);
bool GetGoingUp(void);
float GetCurrentSpeed(void);
void SetMaxSpeed(float speed);
void SetAccel(float new_accel);
void SetEmergStopEnable();

#ifdef	__cplusplus
}
#endif

#endif	/* PHYSICS_H */

