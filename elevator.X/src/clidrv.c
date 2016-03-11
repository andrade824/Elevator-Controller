/**
 * Handles the command-line interface using the FreeRTOS CLI library.
 * 
 * Type "help" while the application is running to view a list of all commands
 */
#include <string.h>
#include <stdlib.h>
#include <FreeRTOS.h>
#include <FreeRTOS_CLI.h>
#include <queue.h>
#include "physics.h"

// The maximum length of the parameter strings
#define MAX_PARAM_LEN 10

// Strings sent to the terminal
static const char taskListHdr[] = "Name\t\tStat\tPri\tS/Space\tTCB\r\n";

// Queue for sending door messages
static QueueHandle_t door_queue;

/**
 * Convert a CLI parameter into an integer
 * 
 * @param commandString The command string passed from the CLI library
 * 
 * @return The integer equivalent of the parameter, or zero if not possible
 */
static int GetIntParam(const char *commandString)
{
    char paramString[MAX_PARAM_LEN];
    const char * param;
    portBASE_TYPE len;
    
    param = FreeRTOS_CLIGetParameter(commandString, 1, &len);
    strncpy(paramString, param, sizeof(paramString));
    paramString[len] = '\0';
    
    return atoi(paramString);
}

/**
 * Convert a CLI parameter into a float
 * 
 * @param commandString The command string passed from the CLI library
 * 
 * @return The float equivalent of the parameter, or 0.0f if not possible
 */
static float GetFloatParam(const char *commandString)
{
    char paramString[MAX_PARAM_LEN];
    const char * param;
    portBASE_TYPE len;
    
    param = FreeRTOS_CLIGetParameter(commandString, 1, &len);
    strncpy(paramString, param, sizeof(paramString));
    paramString[len] = '\0';
    
    return strtof(paramString, NULL);
}

/**
 * Task stats command
 */
static portBASE_TYPE prvTaskStatsCommand(char *pcWriteBuffer, 
                                  size_t xWriteBufferLen,
                                  const char *pcCommandString)
{
    sprintf(pcWriteBuffer, taskListHdr);
    pcWriteBuffer += strlen(taskListHdr);
    vTaskList(pcWriteBuffer);

    return pdFALSE;
}

/**
 * Ground Call command
 */
static portBASE_TYPE prvGDCallCommand(char *pcWriteBuffer, 
                                 size_t xWriteBufferLen,
                                 const char *pcCommandString)
{
    sprintf(pcWriteBuffer, "Floor GD Requested\r\n");
    
    SetRequest(0, UP);
    
    return pdFALSE;
}

/**
 * P1 Down Call command
 */
static portBASE_TYPE prvP1DNCallCommand(char *pcWriteBuffer, 
                                 size_t xWriteBufferLen,
                                 const char *pcCommandString)
{
    sprintf(pcWriteBuffer, "Floor P1 DN Requested\r\n");
    
    SetRequest(1, DOWN);
    
    return pdFALSE;
}

/**
 * P1 UP Call command
 */
static portBASE_TYPE prvP1UPCallCommand(char *pcWriteBuffer, 
                                 size_t xWriteBufferLen,
                                 const char *pcCommandString)
{
    sprintf(pcWriteBuffer, "Floor P1 UP Requested\r\n");
    
    SetRequest(1, UP);
    
    return pdFALSE;
}

/**
 * P2 Call command
 */
static portBASE_TYPE prvP2CallCommand(char *pcWriteBuffer, 
                                 size_t xWriteBufferLen,
                                 const char *pcCommandString)
{
    sprintf(pcWriteBuffer, "Floor P2 Requested\r\n");
    
    SetRequest(2, DOWN);
    
    return pdFALSE;
}

/**
 * Emergency stop command
 */
static portBASE_TYPE prvEmergStopCommand(char *pcWriteBuffer, 
                                 size_t xWriteBufferLen,
                                 const char *pcCommandString)
{
    SetEmergStopEnable();
    sprintf(pcWriteBuffer, "Emergency stop activated\r\n");
    
    return pdFALSE;
}

/**
 * Emergency Clear command
 */
static portBASE_TYPE prvEmergClearCommand(char *pcWriteBuffer, 
                                 size_t xWriteBufferLen,
                                 const char *pcCommandString)
{
    enum DOOR_MSG msg;
    
    if(!GetIsMoving())
    {
        sprintf(pcWriteBuffer, "Door Closing\r\n");
        msg = CLOSE;
        xQueueOverwrite(door_queue, (void*)&msg);
    }
    else
        sprintf(pcWriteBuffer, "wait until the car is stopped before clearing emergency status\r\n");
    
    return pdFALSE;
}

/**
 * Door Interference command
 */
static portBASE_TYPE prvDoorInterferenceCommand(char *pcWriteBuffer, 
                                 size_t xWriteBufferLen,
                                 const char *pcCommandString)
{
    enum DOOR_MSG msg;
    // We have nothing to output so set it to null
    *pcWriteBuffer = NULL;
    
    if(!GetIsMoving())
    {
        sprintf(pcWriteBuffer, "Door Opening\r\n");
        msg = OPEN_CLOSE_SEQ;
        xQueueOverwrite(door_queue, (void*)&msg);
    }
    else
        sprintf(pcWriteBuffer, "Can't open door while car is moving\r\n");
    
    return pdFALSE;
}

/**
 * Change maximum speed command
 */
static portBASE_TYPE prvChangeMaxSpeedCommand(char *pcWriteBuffer, 
                                 size_t xWriteBufferLen,
                                 const char *pcCommandString)
{
    float speed = GetFloatParam(pcCommandString);
    
    sprintf(pcWriteBuffer, "Maximum speed updated\r\n");
    
    SetMaxSpeed(speed);
    
    return pdFALSE;
}

/**
 * Change acceleration command
 */
static portBASE_TYPE prvChangeAccelCommand(char *pcWriteBuffer, 
                                 size_t xWriteBufferLen,
                                 const char *pcCommandString)
{
    float accel = GetFloatParam(pcCommandString);
    
    sprintf(pcWriteBuffer, "Acceleration updated\r\n");
    
    SetAccel(accel);
    
    return pdFALSE;
}

/**
 * Send to floor command
 */
static portBASE_TYPE prvSendToFloorCommand(char *pcWriteBuffer, 
                                 size_t xWriteBufferLen,
                                 const char *pcCommandString)
{
    int floor = GetIntParam(pcCommandString);
    
    if(floor >= 0 && floor <= 2)
    {
        sprintf(pcWriteBuffer, "Floor Requested\r\n");

        if(GetGoingUp())
            SetRequest(floor, UP);
        else
            SetRequest(floor, DOWN);
    }
    else
        sprintf(pcWriteBuffer, "Floor number has to be between 0 and 2\r\n");
    
    return pdFALSE;
}

// Commands available to the user
static const xCommandLineInput xzCommand = {"z",
            "z:\r\n GD Floor Call outside car\r\n\r\n",
            prvGDCallCommand,
            0};

static const xCommandLineInput xxCommand = {"x",
            "x:\r\n P1 Call DN outside car\r\n\r\n",
            prvP1DNCallCommand,
            0};

static const xCommandLineInput xcCommand = {"c",
            "c:\r\n P1 Call UP outside car\r\n\r\n",
            prvP1UPCallCommand,
            0};

static const xCommandLineInput xvCommand = {"v",
            "v:\r\n P2 Call outside car\r\n\r\n",
            prvP2CallCommand,
            0};

static const xCommandLineInput xbCommand = {"b",
            "b:\r\n Emergency Stop inside car\r\n\r\n",
            prvEmergStopCommand,
            0};

static const xCommandLineInput xnCommand = {"n",
            "n:\r\n Emergency Clear inside car\r\n\r\n",
            prvEmergClearCommand,
            0};

static const xCommandLineInput xmCommand = {"m",
            "m:\r\n Door interference\r\n\r\n",
            prvDoorInterferenceCommand,
            0};

static const xCommandLineInput xSCommand = {"S",
            "S n:\r\n Change maximum speed in ft/s\r\n\r\n",
            prvChangeMaxSpeedCommand,
            1};

static const xCommandLineInput xAPCommand = {"AP",
            "AP n:\r\n Change acceleration in ft/s^2\r\n\r\n",
            prvChangeAccelCommand,
            1};

static const xCommandLineInput xSFCommand = {"SF",
            "SF 0/1/2:\r\n Send to floor\r\n\r\n",
            prvSendToFloorCommand,
            1};

static const xCommandLineInput xESCommand = {"ES",
            "ES:\r\n Emergency Stop\r\n\r\n",
            prvEmergStopCommand,
            0};

static const xCommandLineInput xERCommand = {"ER",
            "ER:\r\n Emergency Clear\r\n\r\n",
            prvEmergClearCommand,
            0};

static const xCommandLineInput xTSCommand = {"TS",
            "TS:\r\n Displays a table of task state information\r\n\r\n",
            prvTaskStatsCommand,
            0};

static const xCommandLineInput xRTSCommand = {"RTS",
            "RTS:\r\n Run-time-stats\r\n\r\n",
            prvTaskStatsCommand,
            0};

/**
 * Initialize the Command Line Interface (CLI) subsystem
 */
void InitCLI(QueueHandle_t door_rx_queue)
{
    // Register CLI commands
    FreeRTOS_CLIRegisterCommand(&xzCommand);
    FreeRTOS_CLIRegisterCommand(&xxCommand);
    FreeRTOS_CLIRegisterCommand(&xcCommand);
    FreeRTOS_CLIRegisterCommand(&xvCommand);
    FreeRTOS_CLIRegisterCommand(&xbCommand);
    FreeRTOS_CLIRegisterCommand(&xnCommand);
    FreeRTOS_CLIRegisterCommand(&xmCommand);
    FreeRTOS_CLIRegisterCommand(&xSCommand);
    FreeRTOS_CLIRegisterCommand(&xAPCommand);
    FreeRTOS_CLIRegisterCommand(&xSFCommand);
    FreeRTOS_CLIRegisterCommand(&xESCommand);
    FreeRTOS_CLIRegisterCommand(&xERCommand);
    FreeRTOS_CLIRegisterCommand(&xTSCommand);
    FreeRTOS_CLIRegisterCommand(&xRTSCommand);
    
    // Set door queue
    door_queue = door_rx_queue;
}