#define _SUPPRESS_PLIB_WARNING 1
#define _DISABLE_OPENADC10_CONFIGPORT_WARNING 1

/* Standard includes. */
#include <stdint.h>
#include <plib.h>

/* FreeRTOS includes. */
#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>
#include <queue.h>

/* Hardware include. */
#include <xc.h>

/* My Includes */
#include "leddrv.h"
#include "uartdrv.h"
#include "clidrv.h"
#include "physics.h"
#include "doordrv.h"
#include "btndrv.h"
#include "motordrv.h"

/* Hardware configuration. */
#pragma config FPLLMUL = MUL_20, FPLLIDIV = DIV_2, FPLLODIV = DIV_1, FWDTEN = OFF
#pragma config POSCMOD = HS, FNOSC = PRIPLL, FPBDIV = DIV_2, CP = OFF, BWP = OFF
#pragma config PWP = OFF /*, UPLLEN = OFF, FSRSSEL = PRIORITY_7 */

/* Performs the hardware initialization to ready the hardware to run this example */
static void prvSetupHardware(void);

/*-----------------------------------------------------------*/
int main(void)
{
    /* Perform any hardware initialization that may be necessary. */
    prvSetupHardware();

    // Create the queues
    QueueHandle_t uartQueue = xQueueCreate(20, sizeof(char) * TX_SIZE);
    QueueHandle_t door_rx_queue = xQueueCreate(1, sizeof(enum DOOR_MSG));
    QueueHandle_t door_tx_queue = xQueueCreate(1, sizeof(enum DOOR_MSG));
    
    // Parameters for the tasks
    xUartTaskParameter_t xUartParam = {uartQueue};
    xPhysicsTaskParameter_t xPhysicsParam = {uartQueue, door_rx_queue, door_tx_queue};
    xDoorTaskParameter_t xDoorParam = {door_rx_queue, door_tx_queue};
    xBtnTaskParameter_t xBtnParam = {uartQueue, door_rx_queue};
    
    // Initialize the command line interface
    InitCLI(door_rx_queue);
    
    // Create the tasks
    xTaskCreate(taskPhysics,
            "Physics",
            configMINIMAL_STACK_SIZE,
            (void*)&xPhysicsParam,
            3,
            NULL);
    
    xTaskCreate(taskDoor,
            "Door",
            configMINIMAL_STACK_SIZE,
            (void*)&xDoorParam,
            1,
            NULL);
    
    xTaskCreate(taskButtons,
            "Buttons",
            configMINIMAL_STACK_SIZE,
            (void*)&xBtnParam,
            1,
            NULL);
    
    xTaskCreate(taskMotor,
            "Motor",
            configMINIMAL_STACK_SIZE,
            NULL,
            1,
            NULL);
    
    xTaskCreate(taskUARTRx,
            "UartRx",
            configMINIMAL_STACK_SIZE,
            (void*)&xUartParam,
            4,
            &rx_task);
    
    xTaskCreate(taskUARTTx,
            "UartTx",
            configMINIMAL_STACK_SIZE,
            (void*)&xUartParam,
            2,
            NULL);
    
    /* Start the scheduler so the tasks start executing.  This function should not return. */
    vTaskStartScheduler();
}

/*-----------------------------------------------------------*/
static void prvSetupHardware(void)
{
    /* Setup the CPU clocks, and configure the interrupt controller. */
    SYSTEMConfigPerformance(configCPU_CLOCK_HZ);
    mOSCSetPBDIV(OSC_PB_DIV_2);
    INTEnableSystemMultiVectoredInt();

    initializeLedDriver();
    InitUART(UART1, 9600);
    
    // Motor pin
    mPORTFClearBits(BIT_8);
    mPORTFSetPinsDigitalOut(BIT_8);
    
    // Setup UP/DN Leds
    /* LEDs off. */
    mPORTBClearBits(BIT_4 | BIT_5);

    /* LEDs are outputs. */
    mPORTBSetPinsDigitalOut(BIT_4 | BIT_5);
    
    // Setup switches
    mPORTDSetPinsDigitalIn(BIT_6 | BIT_7 | BIT_13);
    mPORTCSetPinsDigitalIn(BIT_1 | BIT_2);
    
    // Enable pull-ups on switches
    ConfigCNPullups(CN15_PULLUP_ENABLE | CN16_PULLUP_ENABLE | CN19_PULLUP_ENABLE);
}

void vApplicationMallocFailedHook( void )
{
	/* vApplicationMallocFailedHook() will only be called if
	configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
	function that will get called if a call to pvPortMalloc() fails.
	pvPortMalloc() is called internally by the kernel whenever a task, queue,
	timer or semaphore is created.  It is also called by various parts of the
	demo application.  If heap_1.c or heap_2.c are used, then the size of the
	heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
	FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
	to query the size of free heap space that remains (although it does not
	provide information on how the remaining heap might be fragmented). */
	taskDISABLE_INTERRUPTS();
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
	/* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
	to 1 in FreeRTOSConfig.h.  It will be called on each iteration of the idle
	task.  It is essential that code added to this hook function never attempts
	to block in any way (for example, call xQueueReceive() with a block time
	specified, or call vTaskDelay()).  If the application makes use of the
	vTaskDelete() API function (as this demo application does) then it is also
	important that vApplicationIdleHook() is permitted to return to its calling
	function, because it is the responsibility of the idle task to clean up
	memory allocated by the kernel to any task that has since been deleted. */
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time task stack overflow checking is performed if
	configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook	function is 
	called if a task stack overflow is detected.  Note the system/interrupt
	stack is not checked. */
	taskDISABLE_INTERRUPTS();
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationTickHook( void )
{
	/* This function will be called by each tick interrupt if
	configUSE_TICK_HOOK is set to 1 in FreeRTOSConfig.h.  User code can be
	added here, but the tick hook is called from an interrupt context, so
	code must not attempt to block, and only the interrupt safe FreeRTOS API
	functions can be used (those that end in FromISR()). */
}
/*-----------------------------------------------------------*/

void _general_exception_handler( unsigned long ulCause, unsigned long ulStatus )
{
	/* This overrides the definition provided by the kernel.  Other exceptions 
	should be handled here. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vAssertCalled( const char * pcFile, unsigned long ulLine )
{
volatile unsigned long ul = 0;

	( void ) pcFile;
	( void ) ulLine;

	__asm volatile( "di" );
	{
		/* Set ul to a non-zero value using the debugger to step out of this
		function. */
		while( ul == 0 )
		{
			portNOP();
		}
	}
	__asm volatile( "ei" );
}
