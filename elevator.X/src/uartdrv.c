/**
 * Handles sending and receiving UART data through an interrupt-driven interface.
 * 
 * Other tasks will use the queues set up in 'main' to send characters to transmit
 * over the UART. The UART TX task will then read from that queue and transmit the data.
 */
#define _SUPPRESS_PLIB_WARNING 1
#define _DISABLE_OPENADC10_CONFIGPORT_WARNING 1

#include <stdint.h>
#include <string.h>
#include <plib.h>
#include "FreeRTOSConfig.h"
#include <FreeRTOS.h>
#include <timers.h>
#include <semphr.h>
#include "FreeRTOS_CLI.h"
#include "uartdrv.h"

// The UART module to be using
volatile static UART_MODULE uart_module;

// When performing polled transmit IO, delay for this much time
static const TickType_t pollDelay = 2 / portTICK_PERIOD_MS;

// For transmitting a newline
static const char newLine[] = "\r\n";

// Transmit/Receive buffers
static char tx_buffer[TX_SIZE];
static char rx_buffer;

volatile static uint8_t tx_index;

// The handle to resume in the RX interrupt
TaskHandle_t rx_task;

// Mutexes
SemaphoreHandle_t rx_semaphore;
SemaphoreHandle_t tx_semaphore;

// Assembly ISR wrapper
void __attribute__((interrupt(IPL1AUTO), vector(_UART1_VECTOR)))
vUART1_ISR_Wrapper( void );

/**
 * Initialize the UART
 * 
 * @param umPortNum The UART port number
 * @param ui32WantedBaud The baud rate needed
 * @param rx_task The task to resume in the RX interrupt
 */
void InitUART(UART_MODULE umPortNum, uint32_t ui32WantedBaud)
{
    /* Set the Baud Rate of the UART */
    UARTSetDataRate(umPortNum, (uint32_t)configPERIPHERAL_CLOCK_HZ, ui32WantedBaud);
    
    /* Enable the UART for Transmit Only*/
    UARTEnable(umPortNum, UART_ENABLE_FLAGS(UART_PERIPHERAL | UART_RX | UART_TX));

    UARTSetFifoMode(umPortNum, UART_INTERRUPT_ON_RX_NOT_EMPTY | UART_INTERRUPT_ON_TX_DONE);
    
    // Setup interrupt stuff
    INTSetVectorPriority(INT_UART_1_VECTOR, INT_PRIORITY_LEVEL_1);
    INTClearFlag(INT_U1TX);
    INTClearFlag(INT_U1RX);
    INTEnable(INT_U1TX, INT_DISABLED);
    INTEnable(INT_U1RX, INT_ENABLED);
    
    // Set global variables
    uart_module = umPortNum;
    
    // Create Semaphores
    rx_semaphore = xSemaphoreCreateBinary();
    tx_semaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(tx_semaphore);
}

/**
 * Send one character over the UART (polled)
 * 
 * @param umPortNum The UART port number
 * @param cByte The byte to send
 */
void vUartPutC(UART_MODULE umPortNum, char cByte)
{
    // Wait for UART to be ready
    while(!UARTTransmitterIsReady(umPortNum))
        vTaskDelay(pollDelay);
    
    UARTSendDataByte(umPortNum, cByte);
}

/**
 * Send a string over the UART (interrupt)
 * 
 * @param umPortNum The UART port number
 * @param pstring The string to send
 * @param iStrLen The length of the string
 */
void vUartPutStr(UART_MODULE umPortNum, char *pstring, int iStrLen)
{
    xSemaphoreTake(tx_semaphore, portMAX_DELAY);
    strncpy(tx_buffer, pstring, iStrLen);
    tx_buffer[iStrLen] = '\0';
    
    // Enable the interrupt and send the byte
    INTEnable(INT_U1TX, INT_ENABLED);
    tx_index = 1;
    UARTSendDataByte(umPortNum, tx_buffer[0]);
}

/**
 * Return back the last received character
 * 
 * @return The last character received over interrupt
 */
char UartGetChar(void)
{
    return rx_buffer;
}

/**
 * UART Transmit Task
 * 
 * @param pvParameters The UART TX queue
 */
void taskUARTTx(void *pvParameters)
{
    xUartTaskParameter_t *pxTaskParameter;
    char message[TX_SIZE];
    
    /* The parameter points to an xTaskParameters_t structure. */
    pxTaskParameter = (xUartTaskParameter_t *) pvParameters;
    
    while(1)
    {
        // Handle queue messages
        xQueueReceive(pxTaskParameter->tx_queue, (void*)&message, portMAX_DELAY);
        vUartPutStr(UART1, message, strlen(message));
    }
}

/**
 * The UART Receive Task
 * 
 * @param pvParameters The UART TX queue
 */
void taskUARTRx(void *pvParameters)
{
    xUartTaskParameter_t *pxTaskParameter;
    char message[TX_SIZE];   // Sent to UART TX
    char typedChar[2];
    char buffer[100];   // Store the currently typed command
    typedChar[1] = '\0';
    uint16_t buffer_index = 0;
    portBASE_TYPE moreData;
    
    /* The parameter points to an xTaskParameters_t structure. */
    pxTaskParameter = (xUartTaskParameter_t *) pvParameters;
    
    while(1)
    {
        xSemaphoreTake(rx_semaphore, portMAX_DELAY);
        
        // Grab the currently being typed command
        buffer[buffer_index] = UartGetChar();
        typedChar[0] = buffer[buffer_index];
        
        // If its command, process it
        if(buffer[buffer_index] == '\r')
        {
            buffer[buffer_index] = '\0';
            xQueueSendToBack(pxTaskParameter->tx_queue, (void*)&newLine, 0);
            
            do
            {
                moreData = FreeRTOS_CLIProcessCommand(buffer, message, TX_SIZE - 1);
                message[TX_SIZE - 1] = '\0';
                xQueueSendToBack(pxTaskParameter->tx_queue, (void*)&message, 0);
            } while(moreData != pdFALSE);
            
            buffer_index = 0;
            memset(buffer, 0x00, 100);
        }
        else if(buffer[buffer_index] == 0x7F)
        {
            xQueueSendToBack(pxTaskParameter->tx_queue, (void*)&typedChar, 0);
            
            // Handle backspaces
            if(buffer_index > 0)
                buffer_index--;
        }
        else
        {
            switch(typedChar[0])
            {
                // Special command characters
                case 'z': FreeRTOS_CLIProcessCommand(typedChar, message, TX_SIZE - 1); break;
                case 'x': FreeRTOS_CLIProcessCommand(typedChar, message, TX_SIZE - 1); break;
                case 'c': FreeRTOS_CLIProcessCommand(typedChar, message, TX_SIZE - 1); break;
                case 'v': FreeRTOS_CLIProcessCommand(typedChar, message, TX_SIZE - 1); break;
                case 'b': FreeRTOS_CLIProcessCommand(typedChar, message, TX_SIZE - 1); break;
                case 'n': FreeRTOS_CLIProcessCommand(typedChar, message, TX_SIZE - 1); break;
                case 'm': FreeRTOS_CLIProcessCommand(typedChar, message, TX_SIZE - 1); break;
                
                // Any other character
                default:
                    xQueueSendToBack(pxTaskParameter->tx_queue, (void*)&typedChar, 0);
                    buffer_index++;
            }
            
            // Transfer a UART message if needed
            if(typedChar[0] == 'z' || typedChar[0] == 'x' || typedChar[0] == 'c' ||
               typedChar[0] == 'v' || typedChar[0] == 'b' || typedChar[0] == 'n' ||
               typedChar[0] == 'm')
            {
                message[TX_SIZE - 1] = '\0';
                xQueueSendToBack(pxTaskParameter->tx_queue, (void*)&message, 0);
            }
        }
    }
}

/**
 * UART1 Interrupt
 */
void vUART1_ISR(void)
{
    portBASE_TYPE xHigherPriorityTaskWoken;
    
    if(INTGetFlag(INT_U1TX))
    {
        if(tx_buffer[tx_index] == '\0')
        {
            xSemaphoreGiveFromISR(tx_semaphore, &xHigherPriorityTaskWoken);
            INTEnable(INT_U1TX, INT_DISABLED);
        }
        else
        {
            UARTSendDataByte(uart_module, tx_buffer[tx_index]);
            tx_index++;
        }
        
        INTClearFlag(INT_U1TX);
    }
    else if(INTGetFlag(INT_U1RX))
    {
        rx_buffer = UARTGetDataByte(uart_module);
        
        INTClearFlag(INT_U1RX);
        
        xSemaphoreGiveFromISR(rx_semaphore, &xHigherPriorityTaskWoken);
    }
    
    portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
}