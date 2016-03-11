#ifndef BTNDRV_H
#define	BTNDRV_H

#ifdef	__cplusplus
extern "C" {
#endif
#include <FreeRTOS.h>
#include <queue.h>
    
typedef struct xBTN_TASK_PARAMETER {
    QueueHandle_t tx_queue;
    QueueHandle_t door_rx_queue;    // Door receives messages on this queue
} xBtnTaskParameter_t;
    
void taskButtons(void *pvParameters);


#ifdef	__cplusplus
}
#endif

#endif	/* BTNDRV_H */

