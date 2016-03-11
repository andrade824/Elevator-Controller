#ifndef CLIDRV_H
#define	CLIDRV_H

#ifdef	__cplusplus
extern "C" {
#endif
#include <queue.h>
    
// Initialize the Command Line Interface (CLI) subsystem
void InitCLI(QueueHandle_t door_rx_queue);
    
#ifdef	__cplusplus
}
#endif

#endif	/* CLIDRV_H */

