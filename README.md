# Elevator Controller
The goal of this project was to build a three story "express" elevator controller using FreeRTOS on the PIC32 platform. The structure is assumed to have 52 floors; three stop floors at Ground floor (GD) [0 feet], and two penthouse floors P1 [500 feet] and P2 [510 feet]. A passenger is able to call the elevator from the outside on one of the three stop floors. Once inside, the passenger selects one of three floors to ride to. From GD, the car should accelerate to a maximum speed, maintain maximum speed, decelerate to stop at either P1 or P2. The process should be reversed if the direction is down from floors P1 or P2. If traveling between P1 and P2, the car should accelerate for half the floor distance and decelerate to stop for the second half of the distance.

## Design
The design is broken down into the following six modules. Each of these modules contains a FreeRTOS task (besides the command line interface driver) that is created and started in "main". These tasks utilize a combination of queues and getters/setters on global variables to communicate data.

### Physics Task
The physics task is responsible for updating the location and speed of the elevator car over time as floors are requested. It contains two key functions: MoveCar() and UpdateDestination(). MoveCar() is reponsible for updating the speed and location of the elevator car according to its current location and destination. MoveCar() won't return until the car has reached its destination. UpdateDestination() chooses the next destination for the car based on what floor buttons have been pressed. The button and CLI drivers communicate this information through a global SetRequest() function. 

The physics task will keep delaying until a destination is available. Once that occurs, it will call MoveCar() to move to the destination. Once at the destination, the Door driver will take over and open and close the doors. After that, the process starts over again.

### Door Task
The door task handles the opening and closing of the door (as one might guess from the name). The door task receives messages over a queue which tell it when to open and close the door. It then walks through a state machine to handle the door animation. Once the door has closed, it sends out a message over another a queue (queues are only one-way, so two are needed for bi-directional communication) to inform the Physics task of the animation ending. Alternatively, a STAY_OPEN message can be sent to force the doors to stay open until a specific CLOSE message is received. This is useful for handling the emergency stop functionality.

### Button Task
The button task simply polls and debounces the button inputs. It polls the buttons every 100ms, and if it sees a button being pressed down, it waits 15ms and re-checks the state of the button. If the button is still being pressed down, then another chunk of code (dependent on which button is being pressed) will run.

### Motor Task
This task toggles pin RF8 at 1Hz for every 10ft/sec of travel speed (to simulate driving a motor). The physics task provides a getter function to retreive the current speed.

### UART RX and TX Tasks
These tasks handle interrupt-driven receive and transmit operations for the UART. The transmit task contains a queue which the rest of the system uses to tell the UART driver to transmit data. The receive task will buffer each incoming character until either a "\r" ("enter" keypress) or keyboard command is received. If a "\r" is received, then the command line interface driver is invoked to perform the required operation. If a keyboard command is detected (as outlined below) then the command is processed without the need for pressing "return".

### Command Line Interface (CLI) Driver
The CLI driver utilizes the FreeRTOS+CLI library to create a command line interface for this controller. The UART receive task handles receiving and formatting the commands for the CLI driver. After that, the CLI library is invoked and the appropriate command is executed.

## Inputs and Outputs
The controller will take inputs from both the UART reciever and buttons available on the development kit (the <a href="http://www.microchip.com/Developmenttools/ProductDetails.aspx?PartNO=DM320001" target="_blank">PIC32 starter kit</a> and <a href="http://www.eflightworks.net/P32_starter_companion.html" target="_blank">companion board</a> are being used). The inputs and outputs are outlined below:

UART RX
<ul>
	<li>Keyboard "z" - GD Floor Call outside car</li>
	<li>Keyboard "x" - P1 DN Floor Call outside car</li>
	<li>Keyboard "c" - P1 UP Floor Call outside car</li>
	<li>Keyboard "v" - P2 Floor Call outside car</li>
	<li>Keyboard "b" - Emergency Stop inside car</li>
	<li>Keyboard "n" - Emergency Clear inside car</li>
	<li>Keyboard "m" - Door interference</li>
</ul>

UART TX
<ul>
	<li>Floor indicator reached Status: "Floor [GD/P1/P2]" "[Stopped/Moving]"</li>
	<li>Distance from ground level and Current Speed Status: "n Feet:: m ft/s" every 500 ms</li>
</ul>

Starter Kit Buttons
<ul>
	<li>SW1 - P2 button inside car</li>
	<li>SW2 - P1 button inside car</li>
	<li>SW3 - GD button inside car</li>
</ul>

Starter Kit LEDs
<ul>
	<li>LED1-LED3 - Door Simulator</li>
</ul>
	
Companion Board Buttons
<ul>
	<li>RC1 - Open Door inside car</li>
	<li>RC2 - Close Door inside car</li>
</ul>

Companion Board LEDs
<ul>
	<li>D7 (LED1) - UP Indicator</li>
	<li>D6 (LED2) - DN Indicator</li>
</ul>

Commands (via Command Line Interface)
<ul>
	<li>[S n] Change Maximum Speed in ft/s</li>
	<li>[AP n] Change Acceleration in ft/s2</li>
	<li>[SF 1/2/3] Send to floor</li>
	<li>[ES] Emergency Stop (identical to Emergency Stop Button)</li>
	<li>[ER] Emergency Clear (identical to Emergency Clear Button)</li>
	<li>[TS] Task-states</li>
	<li>[RTS] Run-time-stats</li>
</ul>
