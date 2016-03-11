#ifndef LEDDRV_H
#define	LEDDRV_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#include <stdint.h>

#define LED1 0
#define LED2 1
#define LED3 2

uint8_t initializeLedDriver(void);
uint8_t readLed(uint8_t ledNum);
uint8_t setLED(uint8_t ledNum, uint8_t value);
uint8_t toggleLED(uint8_t ledNum);

#ifdef	__cplusplus
}
#endif

#endif	/* LEDDRV_H */

