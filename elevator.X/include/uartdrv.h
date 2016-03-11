#ifndef UARTDRV_H
#define	UARTDRV_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#include <stdint.h>
#include <plib.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

// Size of transmit buffer in characters
#define TX_SIZE 200

typedef struct xUART_TASK_PARAMETER {
    QueueHandle_t tx_queue;
} xUartTaskParameter_t;

// Initialize the UART
void InitUART(UART_MODULE umPortNum, uint32_t ui32WantedBaud);

// Send one character over the UART
void vUartPutC(UART_MODULE umPortNum, char cByte);

// Send a string over the UART
void vUartPutStr(UART_MODULE umPortNum, char *pstring, int iStrLen);

// Retrieve the last character recieved by the ISR
char UartGetChar(void);

// UART Tasks
void taskUARTRx(void *pvParameters);
void taskUARTTx(void *pvParameters);

// The receive task handle
extern TaskHandle_t rx_task;
    
#ifdef	__cplusplus
}
#endif

#endif	/* UARTDRV_H */

