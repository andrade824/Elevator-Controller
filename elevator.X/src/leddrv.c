/**
 * Basic driver file for manipulating the three LEDs on the PIC32 starter kit board.
 */
#define _SUPPRESS_PLIB_WARNING 1
#define _DISABLE_OPENADC10_CONFIGPORT_WARNING 1

#include <stdint.h>
#include <plib.h>
#include <xc.h>

/*
 * This function will setup the ports for the LEDs and set them to the OFF state.
 * Returns a 0 for success, any other value for failure
 */
uint8_t initializeLedDriver(void)
{
    /* LEDs off. */
    mPORTDClearBits(BIT_0 | BIT_1 | BIT_2);

    /* LEDs are outputs. */
    mPORTDSetPinsDigitalOut(BIT_0 | BIT_1 | BIT_2);
    
    return 0;
}

/**
 * This function will return the current state of the given LED Number. 0 for off, 1 for
 * on, any other number is an error condition
 */
uint8_t readLed(uint8_t ledNum)
{
    return mPORTDReadBits(1 << ledNum);
}

/**
 * Sets ledNum to a state of OFF or ON depending on value. If value is 0 turn OFF
 * LED, any other value will turn ON LED. Returns a 0 for success or any other
 * number for failure
 */
uint8_t setLED(uint8_t ledNum, uint8_t value)
{
    if(value == 0)
        mPORTDClearBits(1 << ledNum);
    else
        mPORTDSetBits(1 << ledNum);
    
    return 0;
}

/**
 * This function will toggle the current state of the LED. If the LED is OFF it will turn
 * it ON, if LED is ON it will turn it OFF. Returns 0 for success, any other value for
 * error.
 */
uint8_t toggleLED(uint8_t ledNum)
{
    mPORTDToggleBits(1 << ledNum);
    
    return 0;
}