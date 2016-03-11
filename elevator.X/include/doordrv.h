#ifndef DOORDRV_H
#define	DOORDRV_H

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct xDOOR_TASK_PARAMETER {
    QueueHandle_t door_rx_queue;    // Door receives messages on this queue
    QueueHandle_t door_tx_queue;    // Door transmits messages on this queue
} xDoorTaskParameter_t;

// Door messages sent through queue
enum DOOR_MSG { OPEN_CLOSE_SEQ, STAY_OPEN, CLOSE, CLOSED };

// Door Task
void taskDoor(void *pvParameters);
bool GetDoorClosed();

#ifdef	__cplusplus
}
#endif

#endif	/* DOORDRV_H */

