 /*
 * A controller for linear actuator roof motor controller from the INDI rolloffino roof driver.   
 * 
 * tg August 2018  Original
 * tg February 2020 Generalize to make less installation specific
 *                  Add unspecified usage AUX switch and relay
 *                  Communication protocol in terms of function not switches/relays
 *                  ~ 15KB 2% of Due, 50% of Nano
 * tg November 2021 Break out commandReceived and requestReceived to make alernate actions more 
 *                  obvious/accessible, Remove Due specific code.
 * gg September 2022 Modifications to control a linear actuator based roof
 * 
 * tg: Tom Gibson
 * gg: Gilles Gagnon
 */

/*
 * This version of the rolloff.ino has been modified to work with linear actuators that turn
 * themselves off when they reach their full extension and require the power to be 'inverted'
 * (+tive to -tive and -tive to +tive) so they can be retracted.
 * Review additions to the other Arduino samples to see if they are a more appropriate base.
 * The auxiliary button and light in the remote driver are to turn On or OFF the observatory lights. 
*/


#define BAUD_RATE 38400

# define OPEN_CONTACT HIGH    // Switch definition, Change to LOW if pull-down resistors are used.

// Define name to pin assignments
#define SWITCH_1 3
#define SWITCH_2 2
//#define SWITCH_3 A2
//#define SWITCH_4 A3

#define RELAY_1 8             // Actuator Power GG
#define RELAY_2 9             // Direction GG
#define RELAY_3 7             // Observatory Lights on FUNC_AUX GG
#define RELAY_4 6             // Safety Blinker GG

// Indirection to define a functional name in terms of a switch
// Use 0 if switch not implemented
#define SWITCH_OPENED SWITCH_1  // Fully opened is assigned to switch 1
#define SWITCH_CLOSED SWITCH_2  // Fully closed is assigned to switch 2
#define SWITCH_LOCKED 0         // External lock
#define SWITCH_AUX    0         // Auxiliary switch

// Indirection to define a functional name in terms of a relay
// Use 0 if function not supportd
#define FUNC_ACTIVATION  RELAY_1    // Activation relay connected to the direction relay GG
#define FUNC_DIRECTION   RELAY_2    // Direction relay inverts the power for either actuator extension or retraction GG
#define FUNC_STOP        RELAY_1    // FUNC_STOP (abort) needs only to operatre activation relay GG
#define FUNC_LOCK        0          // If automated roof lock is available.
#define FUNC_AUX         RELAY_3    // Relay to turn ON or OFF observatory lights GG
#define FUNC_BLINKER     RELAY_4    // Relay to turn safety  on/off GG

/*
 * Abort (stop) request is only meaningful if roof is in motion.
 *
 * On Abort for a single button controller, only want to activate relay and pulse the controller if
 * the roof is still moving, then it would stop. If it has already reached the end of its
 * travel, a pulse could set it off again in the opposite direction.
 *
 * In case the end of run switches are not reached, some way to know if it is moving
 * would be helpful. Short of that estimate how long it takes the roof to open or close
 */
#define ROOF_OPEN_MILLI 60000

/*
 * GG
 * Delays required  1) before checking limit switches when the roof opens and 
 *                  2) before turning the power off, after the limit switches are activated
 * May need to be adjusted
 */
#define ROOF_MOVEMENT_MIN_TIME_MILLIS  8000       
#define ROOF_MOTION_END_DELAY_MILLIS  8000       

// Buffer limits
#define MAX_INPUT 45
#define MAX_RESPONSE 127
#define MAX_MESSAGE 63

enum cmd_input {
CMD_NONE,  
CMD_OPEN,
CMD_CLOSE,
CMD_STOP,
CMD_LOCK,
CMD_AUXSET,
CMD_CONNECT,
CMD_DISCONNECT
} command_input;

unsigned long timeMove = 0;
unsigned long MotionStartTime = 0;  // Related to ROOF_MOVEMENT_MIN_TIME_MILLIS GG
unsigned long MotionEndDelay;       // Related to ROOF_MOTION_END_DELAY_MILLIS GG

const int cLen = 15;
const int tLen = 15;
const int vLen = MAX_RESPONSE;
char command[cLen+1];
char target[tLen+1];
char value[vLen+1];

//  Maximum length of messages = 63                                               *|
const char* ERROR1 = "The controller response message was too long";
const char* ERROR2 = "The controller failure message was too long";
const char* ERROR3 = "Command input request is too long";
const char* ERROR4 = "Invalid command syntax, both start and end tokens missing"; 
const char* ERROR5 = "Invalid command syntax, no start token found";
const char* ERROR6 = "Invalid command syntax, no end token found";
const char* ERROR7 = "Roof controller unable to parse command";
const char* ERROR8 = "Command must map to either set a relay or get a switch";
const char* ERROR9 = "Request not implemented in controller";
const char* ERROR10 = "Abort command ignored, roof already stationary";

const char* VERSION_ID = "V1.2-0";

void sendAck(char* val)
{
  char response [MAX_RESPONSE];
  if (strlen(val) > MAX_MESSAGE)
    sendNak(ERROR1);
  else
  {  
    strcpy(response, "(ACK:");
    strcat(response, target);
    strcat(response, ":");
    strcat(response, val);
    strcat(response, ")");
    Serial.println(response);
    Serial.flush();
  }
}

void sendNak(const char* errorMsg)
{
  char buffer[MAX_RESPONSE];
  if (strlen(errorMsg) > MAX_MESSAGE)
    sendNak(ERROR2);
  else
  {
    strcpy(buffer, "(NAK:ERROR:");
    strcat(buffer, value);
    strcat(buffer, ":");
    strcat(buffer, errorMsg);
    strcat(buffer, ")");
    Serial.println(buffer);
    Serial.flush();
  }
}

/*
 * Get switch value
 * NO contacts used and configured with a pull up resistor to the Arduino input pin. Logical 1 input when switch opened. GG
 * When switch closes The LOW voltage logical 0 is applied to the input pin. GG
 * The off or on value is to be sent to the host in the ACK response
 */
void getSwitch(int id, char* value)
{
  if (digitalRead(id) == OPEN_CONTACT)
    strcpy(value, "ON");  // rolloff.ino.standard was OFF
  else
    strcpy(value, "OFF"); // rolloff.ino.standard was ON
}

bool isSwitchOn(int id)
{
  char switch_value[16+1];
  getSwitch(id, switch_value);
  if (strcmp(switch_value, "ON") == 0)
  {
    return true;
  }   
  return false;
}

bool parseCommand()           // (command:target:value)
{
  bool start = false;
  bool eof = false;
  int recv_count = 0;
  int wait = 0;
  int offset = 0;
  char startToken = '(';
  char endToken = ')';
  const int bLen = MAX_INPUT;
  char inpBuf[bLen+1];

  memset(inpBuf, 0, sizeof(inpBuf));
  memset(command, 0, sizeof(command));
  memset(target, 0, sizeof(target));
  memset(value, 0, sizeof(value));
    
  while (!eof && (wait < 20))
  {
    if (Serial.available() > 0)
    {
      Serial.setTimeout(1000);
      recv_count = Serial.readBytes((inpBuf + offset), 1);
      if (recv_count == 1)
      {
        offset++;
        if (offset >= MAX_INPUT)
        {
          sendNak(ERROR3);
          return false;        
        }
        if (inpBuf[offset-1] == startToken)
        {
          start = true;  
        }
        if (inpBuf[offset-1] == endToken) 
        {
          eof = true;
          inpBuf[offset] = '\0';           
        }
        continue;
      }
    }
    wait++;
    delay(100);
  }
    
  if (!start || !eof)
  {
    if (!start && !eof)  
      sendNak(ERROR4);
    else if (!start)
      sendNak(ERROR5);
    else if (!eof)
      sendNak(ERROR6);
    return false;
  }
  else
  {
    strcpy(command, strtok(inpBuf,"(:"));
    strcpy(target, strtok(NULL,":"));
    strcpy(value, strtok(NULL,")"));
    if ((strlen(command) >= 3) && (strlen(target) >= 1) && (strlen(value) >= 1))
    {
      return true;
    }
    else
    {  
      sendNak(ERROR7); 
      return false;
    }
  }              
}

/*
 * Use the parseCommand routine to decode message
 * Determine associated action in the message. Resolve the relay or switch associated 
 * pin with the target identity. Acknowledge any initial connection request. Return 
 * negative acknowledgement with message for any errors found.  Dispatch to commandReceived
 * or requestReceived routines to activate the command or get the requested switch state
 */
void readUSB()
{
  // Confirm there is input available, read and parse it.
  if (Serial && (Serial.available() > 0))
  {
    if (parseCommand())
    {
      unsigned long timeNow = millis();
      int hold = 0;
      int relay = -1;   // -1 = not found, 0 = not implemented, pin number = supported
      int sw = -1;      //      "                 "                    "
      bool connecting = false;
      const char* error = ERROR8;

      // On initial connection return the version
      if (strcmp(command, "CON") == 0)
      {
        connecting = true; 
        strcpy(value, VERSION_ID);  // Can be seen on host to confirm what is running
        commandReceived(CMD_CONNECT, value);
//        sendAck(value);
      }

      // Map the general input command term to the local action
      // SET: OPEN, CLOSE, ABORT, LOCK, AUXSET
      else if (strcmp(command, "SET") == 0)
      {
        // Prepare to OPEN
        if (strcmp(target, "OPEN") == 0)                     
        {
          command_input = CMD_OPEN;
          relay = FUNC_ACTIVATION;
          timeMove = timeNow;
        }
        // Prepare to CLOSE
        else if (strcmp(target, "CLOSE") == 0)    
        { 
          command_input = CMD_CLOSE;
          relay = FUNC_ACTIVATION;
          timeMove = timeNow;
        }
        // Prepare to ABORT
        else if (strcmp(target, "ABORT") == 0)
        {          
          command_input = CMD_STOP;

          // Test whether or not to Abort
          if (!isStopAllowed())
          {           
            error = ERROR10;
          }
          else
          {             
            relay = FUNC_STOP;
          }
        }
        // Prepare for the Lock function
        else if (strcmp(target, "LOCK") == 0)
        { 
          command_input = CMD_LOCK;
          relay = FUNC_LOCK;
        }

        // Prepare for the Auxiliary function
        else if (strcmp(target, "AUXSET") == 0)
        { 
          command_input = CMD_AUXSET;
          relay = FUNC_AUX;
        }
      }

      // Handle requests to obtain the status of switches   
      // GET: OPENED, CLOSED, LOCKED, AUXSTATE
      else if (strcmp(command, "GET") == 0)
      {
        if (strcmp(target, "OPENED") == 0)
          sw = SWITCH_OPENED;
        else if (strcmp(target, "CLOSED") == 0) 
          sw = SWITCH_CLOSED;
        else if (strcmp(target, "LOCKED") == 0) 
          sw = SWITCH_LOCKED;
        else if (strcmp(target, "AUXSTATE") == 0) 
          sw = SWITCH_AUX;
      }
    
      /*
       * See if there was a valid command or request 
       */
      if (!connecting)
      {
        if ((relay == -1) && (sw == -1))
        {
          sendNak(error);               // Unknown input or Abort command was rejected
        }

        // Command or Request not implemented
        else if ((relay == 0 || relay == -1) && (sw == 0 || sw == -1))
        {
          strcpy(value, "OFF");          // Request Not implemented
          //sendNak(ERROR9);   
          sendAck(value);
        }

        // Valid input received
        
        // A command was received
        // Set the relay associated with the command and send acknowlege to host
        else if (relay > 0)            // Set Relay response
        {
          commandReceived(command_input, value);
        }
        
        // A state request was received
        else if (sw > 0)               // Get switch response
        {
          requestReceived(sw);    
        }
      } // end !connecting
    }   // end command parsed
  }     // end Serial input found  
}


////////////////////////////////////////////////////////////////////////////////
// Abort movement command received, test to see if abort is allowed.
// If not return false and an error message will be returned to the host. If yes then return true. 
// If either fully open or fully closed switches are on then deny the request by returning false.
// If neither switch is on then if there is a specific button (relay) assigned that can stop movement then return true
// to allow it to do so.
//
// This implementation assumes a one button setup and one which does not know if the roof is still moving or 
// has hit something and already stopped. Before taking action see how long it has been since movement was initiated.
// If it is longer than the estimate to open or close the roof, assume motion has already stopped. In this case avoid
// emulating the single button push because that would set the roof moving again. If it seems that the roof 
// could be moving then return true.
// 
// Returning true will cause the Abort request to appear in the commandReceived routine where it will activate
// the requested relay. 
// 
bool isStopAllowed()
{
  unsigned long timeNow = millis();
  
  // If the roof is either fully opened or fully closed, ignore the request.
  if (isSwitchOn(SWITCH_OPENED) || isSwitchOn(SWITCH_CLOSED)) 
  {
    return false;
  }

  // If time since last open or close request is longer than the time for the roof travel return false
  if ((timeNow - timeMove) >= ROOF_OPEN_MILLI)
  {
    return false;
  }
  else

  // Stop will be attempted
  {
    return true;
  }
}


////////////////////////////////////////////////////////////////////////////////
// Action command received

// Here after pin associations resolved and request action known
// Default action is to set the associated relay to the requested state "ON" or "OFF" and
// send acknowledgement to the host. 
// target is the name associated with the relay "OPEN", "CLOSE", "STOP", "LOCK", "AUXSET".
// It will be used when  sending the acknowledgement to the host. Find out if a particular 
// command is being processed using if (strcmp(target, "OPEN") == 0) {do something}
//
// relay: pin id of the relay 
// hold:  whether relay is to be set permanently =0, or temporarily =1 (not used in this firmware GG)
// value: How to set the relay "ON" or "OFF" 
// 
//
void commandReceived(int command_input, char* value)
{
  // Stop
  if (command_input == CMD_STOP) {
    
    digitalWrite(FUNC_ACTIVATION, LOW);  
    digitalWrite(FUNC_DIRECTION, LOW);  
    digitalWrite(FUNC_BLINKER, LOW);  

  } else  // Resume Parsing

  // Connect
  if (command_input== CMD_CONNECT) {
    
    digitalWrite(FUNC_ACTIVATION, LOW);  
    digitalWrite(FUNC_DIRECTION, LOW);  
    digitalWrite(FUNC_BLINKER, LOW);  

  } else  // Resume Parsing

  // AUX Set
  if (command_input== CMD_AUXSET) {

    if(strncmp(value, "ON", 2) ) digitalWrite(FUNC_AUX, LOW);  
    if(strncmp(value, "OFF", 3) ) digitalWrite(FUNC_AUX, HIGH);  

  } else  // Resume Parsing

  // Open
  if (command_input == CMD_OPEN) {
    
    digitalWrite(FUNC_BLINKER, HIGH);       // Blink when opening roof
    digitalWrite(FUNC_DIRECTION, LOW);      // Set actuator voltage leads to open actuator
    digitalWrite(FUNC_ACTIVATION, HIGH);    // Set actuator in motion

    MotionStartTime = millis();
      
  } else  // Resume Parsing
  
  // Close
  if (command_input == CMD_CLOSE) {

    digitalWrite(FUNC_BLINKER, HIGH);         // Blink when closing roof
    digitalWrite(FUNC_DIRECTION, HIGH);       // Set actuator voltage leads to close actuator
    digitalWrite(FUNC_ACTIVATION, HIGH);      // Set actuator in motion

    MotionStartTime = millis();
      
  }

  sendAck(value);         // Send acknowledgement that relay pin associated with "target" was activated to value requested
}

////////////////////////////////////////////////////////////////////////////////

// Here after pin associations resolved and request action known
// Request to obtain state of a switch
// Default action is to read the associated switch and return result to the host
// target is the name associated with the switch "OPENED", "CLOSED" etc and will
// be used when sending the acknowledgement to the host. Find out if a certain request is being processed using
// if (strcmp(target, "OPENED") == 0) {do something}
//
// sw:     The switch's pin identifier.
// value   getSwitch will read the pin and set this to "ON" or "OFF" 
void requestReceived(int sw)
{
  getSwitch(sw, value);

  sendAck(value);            // Send result of reading pin associated with "target" 
}


// Check if roof has fully opened or fully closed and turn off relay if so! GG

void check_roof_turn_off_relays(){
  
  if( MotionEndDelay == 0 ) {
    if( MotionStartTime != 0 ) {
      if ( ( millis() - MotionStartTime ) > ROOF_MOVEMENT_MIN_TIME_MILLIS ) {
        if( !(!isSwitchOn(SWITCH_OPENED) && !isSwitchOn(SWITCH_CLOSED) ) ) {
          MotionEndDelay = millis();
        }
      }
    }
  } else { // Add some delay for complete roof opening or closure
    if( ( millis() - MotionEndDelay ) > ROOF_MOTION_END_DELAY_MILLIS ) {
      digitalWrite(FUNC_ACTIVATION, LOW);  
      digitalWrite(FUNC_DIRECTION, LOW);  
      digitalWrite(FUNC_BLINKER, LOW);
      MotionEndDelay = 0;
    }
     MotionStartTime = 0;
  }

}


// One time initialization
void setup() 
{
  // Initialize the input switches
  pinMode(SWITCH_1, INPUT);     // External pullup used GG
  pinMode(SWITCH_2, INPUT);     // External pullup used GG
//  pinMode(SWITCH_1, INPUT_PULLUP); 
//  pinMode(SWITCH_2, INPUT_PULLUP); 
//  pinMode(SWITCH_3, INPUT_PULLUP); 
//  pinMode(SWITCH_4, INPUT_PULLUP);

  // Initialize the relays
  //Pin Setups
  pinMode(RELAY_1, OUTPUT);
  pinMode(RELAY_2, OUTPUT);
  pinMode(RELAY_3, OUTPUT);
  pinMode(RELAY_4, OUTPUT); 

  //Turn Off the relays.
  digitalWrite(RELAY_1, LOW);
  digitalWrite(RELAY_2, LOW);
  digitalWrite(RELAY_3, LOW);
  digitalWrite(RELAY_4, LOW);

  MotionStartTime = 0;
  MotionEndDelay = 0;

  // Establish USB port.
  Serial.begin(BAUD_RATE);    // Baud rate to match that in the driver
}

// Wait here for command or switch request from host
void loop() 
{   

  check_roof_turn_off_relays(); // GG
  
  while (Serial.available() <= 0) 
  {
    for (int cnt=0; cnt < 60; cnt++)
    {
      if (Serial.available() > 0)
        break;
      else
        delay(100);
    }
  }
  readUSB();

}       // end loop


  
