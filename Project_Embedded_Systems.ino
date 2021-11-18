#include "TinyGPS++.h"                                                        //Library for GPS module 
#include "SoftwareSerial.h"                                                   //Library for using pins, other than default (RX,TX) pins present on arduino, as reciever and transmitter pins : (rx,tx)
#include "MAX30100.h"                                                         //Library used for setting up MAX30100 sensor 
#include "MAX30100_PulseOximeter.h"                                           //Library for using MAX30100 sensor as pulse oximeter

SoftwareSerial gsm(3, 4);                                                     //Setting the pins Digital (3,4) as serial pins for communication with gsm
SoftwareSerial gps_connection(5,6);                                           //Setting the pins Digital (5,6) as serial pins for communication with gps
TinyGPSPlus gps;

#define SAMPLING_RATE      MAX30100_SAMPRATE_100HZ                            // Setting the sample rate at which analog values are measured
#define IR_LED_CURRENT     MAX30100_LED_CURR_50MA                             // LED currents set to a level that avoids clipping, IR light used to measure heart rate
#define RED_LED_CURRENT    MAX30100_LED_CURR_27_1MA                           // Combination of red light and infrared light used to measure Oxygen levels
#define PULSE_WIDTH        MAX30100_SPC_PW_1600US_16BITS                      // Used to set the pulse width of the LED, which helps in defining the 
#define HIGHRES_MODE       true                                               // resolution of the ADC

MAX30100 sensor;                                                              // Assigning the library to a variable
PulseOximeter pox;                                                            // Assigning the library to a variable

enum state                                                                    // Predefining the list of values that "State" can take while extracting the message 
{
  DETECT_MSG_TYPE,
  IGNORING_COMMAND_ECHO,
  READ_CMTI_STORAGE_TYPE,
  READ_CMTI_ID,
  READ_CMGR_STATUS,
  READ_CMGR_NUMBER,
  READ_CMGR,
  READ_CMGR_CONTENT
};

byte state = DETECT_MSG_TYPE;                                                 // Setting the state to detect the message type initially
char arr[80];                                                                 // Defining array used to read the message
byte pos = 0;                                                                 // Used to define the position in the array a
int lastReceivedSMSId = 0;                                                    // Used to set the message ID correspondng to the message that is to be deleted at the end
boolean validSender = true;                                                   // Variable that validates the number through which the request for status is sent 
int j;
int l=0;

// Making the complete array an array of zeros to avoid presence of garbage values

void resetarr() 
{
  for (j=0;j<80;j++)
  {
    arr[j]=0;
  }
  pos = 0;                                                                    // Making position = 0 every time the function resetarr is called
}

void setup()
{ 
  pinMode(7, OUTPUT);                                                         // Setting up the pin for buzzer
  pinMode(12, INPUT);                                                         // Setting up the pin for push button

// Beginning all the serial connections

  Serial.begin(9600);
  gps_connection.begin(9600);
  gsm.begin(9600);

// Deleting any previously stored messages upto message ID: 25 (Airtel capacity) 

  for (int i = 1; i <= 25; i++) {
    gsm.print("AT+CMGD=");                                                    // AT + CMGD = ID, deletes the corresponding message
    gsm.println(i);
    delay(200);
    while(gsm.available())                                                    // Serial(gsm).available returns the number of bytes that are used in the serial buffer, once the serial communication is completed its value becomes 0
      Serial.write(gsm.read());                                               // Prints the command issued for gsm into serial monitor one byte after another. Every character requires one byte to store its value
  }

// Initializing the sensor and setting its parameters

  sensor.begin();                                                        
  pox.begin();
  sensor.setMode(MAX30100_MODE_SPO2_HR);
  sensor.setLedsCurrent(IR_LED_CURRENT, RED_LED_CURRENT);
  sensor.setLedsPulseWidth(PULSE_WIDTH);
  sensor.setSamplingRate(SAMPLING_RATE);
  sensor.setHighresModeEnabled(HIGHRES_MODE);
}

void loop()
{
  if (digitalRead(12) == HIGH)                                                // If pushbutton is pressed the function help() is called
  {
    help();
  }
  while(gsm.available())                                                      // Similarly when message is recieved the program enters the loop. At this time gsm.read() reads the message byte wise and text() function is called.
  {
    if (validSender == true)                                                  // Initially set true, later on checked for validity in text function()
    {
      text(gsm.read());
    }
  }
}

// Reading the message

//Message format:
//+CMTI: "SM", 3 
//AT+CMGR=3
//+CMGR: "REC READ","+85291234567",,"07/02/18,00:12:05+32"
//Hello, welcome to our SMS tutorial.

void text(byte b)                                                             // Message is read byte wise
{
  arr[pos++] = b;

  if ( pos >= sizeof(arr))
    resetarr(); 

  switch (state)                                                              // Initial state set as DETECT_MSG_TYPE in the code above
  {
    case DETECT_MSG_TYPE: 
      {
        if ( b == '\n' )                                                      // Reset the array if new line character found
          resetarr();
        else {        
          if ( pos == 3 && strcmp(arr, "AT+") == 0 ) {                        // To avoid reading command echo such as AT+CMGR=ID, strcmp retrns the difference between the ascii values of first and second string
            state = IGNORING_COMMAND_ECHO;
          }
          else if ( pos == 6 ) {
  
            if ( strcmp(arr, "+CMTI:") == 0 ) {                               // +CMTI helps in finding out the ID number and hence deleting the corresponding message finally
              state = READ_CMTI_STORAGE_TYPE;
            }
            else if ( strcmp(arr, "+CMGR:") == 0 ) {                          // +CMGR helps in finding the phone number from which the message was sent
              state = READ_CMGR_STATUS;
            }
            resetarr();
          }
        }
      }
    break;                                                                    // To exit from the switch case loop
  
    case IGNORING_COMMAND_ECHO:
      {
        if ( b == '\n' ) 
        {
          state = DETECT_MSG_TYPE;
          resetarr();
        }
      }
    break;
  
    case READ_CMTI_STORAGE_TYPE:                                              // After , is the ID value
      {
        if ( b == ',' ) 
        {
          state = READ_CMTI_ID;
          resetarr();                                                         // Resetting so that only after , is read
        }
      }
    break;
  
    case READ_CMTI_ID:
      {
        if ( b == '\n' ) {                                                    // Reading till the new line character for message ID
          lastReceivedSMSId = atoi(arr);                                      // atoi - converts string to integer type
          gsm.print("AT+CMGR=");
          gsm.println(lastReceivedSMSId);
          state = DETECT_MSG_TYPE;
          resetarr();
        }
      }
    break;
  
    case READ_CMGR_STATUS:
      {
        if ( b == ',' ) 
        {
          state = READ_CMGR_NUMBER;
          resetarr();
        }
      }
    break;
  
    case READ_CMGR_NUMBER:
      {
        if ( b == ',' ) {                                                     // Reads till , to find out the phone number
          Serial.print("CMGR number: ");
          Serial.println(arr);
          
// Checking for validity, with the number fed in the program

          if ( strcmp(arr, "\"+919978487678\",") == 0 )
          {        
            validSender = true;
          }
          else
          {
            validSender = false;
            Serial.println("Invalid Number");
          }
  
          state = READ_CMGR;
          resetarr();
        }
      }
    break;
  
    case READ_CMGR:                                                           // Finally for reading the message content we move to the next line
      {
        if ( b == '\n' ) 
        {
          state = READ_CMGR_CONTENT;
          resetarr();
        }
      }
    break;
  
    case READ_CMGR_CONTENT:                                                   // We read till the new line character occurs for reading the message
      {
        if ( b == '\n' ) {
          Serial.print("CMGR content: ");
          Serial.print(arr);

// Checking the message content recieved
          
          if (((arr[0] == 'S') || (arr[0] == 's')) && ((arr[1] == 'T') || (arr[1] == 't')) && ((arr[2] == 'A') || (arr[2] == 'a')) && ((arr[3] == 'T') || (arr[3] == 't')) && ((arr[4] == 'U') || (arr[4] == 'u')) && ((arr[5] == 'S') || (arr[5] == 's')))
          {
            stats();                                                          // If status, reverting back with location and health parameters
          }
          else if(((arr[0] == 'B') || (arr[0] == 'b')) && ((arr[1] == 'U') || (arr[1] == 'u')) && ((arr[2] == 'Z') || (arr[2] == 'z')) && ((arr[3] == 'Z') || (arr[3] == 'z')) && ((arr[4] == '_')) && ((arr[5] == 'O') || (arr[5] == 'o')) && ((arr[6] == 'N') || (arr[6] == 'n')))
          {
            buzzon();                                                         // If buzz_on, calling function buzzon to turn on the buzzer
          }
          else if(((arr[0] == 'B') || (arr[0] == 'b')) && ((arr[1] == 'U') || (arr[1] == 'u')) && ((arr[2] == 'Z') || (arr[2] == 'z')) && ((arr[3] == 'Z') || (arr[3] == 'z')) && ((arr[4] == '_')) && ((arr[5] == 'O') || (arr[5] == 'o')) && ((arr[6] == 'F') || (arr[6] == 'f')) && ((arr[6] == 'F') || (arr[6] == 'f')))
          {
            buzzoff();                                                        // If buzz_off, calling function buzzoff to turn off the buzzer
          }
          else
          {
            Serial.println("Invalid message");
          }

// Deleting the message received

          gsm.print("AT+CMGD=");
          gsm.println(lastReceivedSMSId);
          state = DETECT_MSG_TYPE;
          resetarr();
        }
      }
    break;
  }
}

void stats() 
{
  gps_connection.listen();                                                    // Enabling gsp serial communication, which disables gsm serial communication
  while (1)
  {
    while (gps_connection.available())                                        // Once the gps locks with the satellite the coordinates are serially communicated, the locking occurs for certain time than it breaks and the process keeps on repeating
    {
      gps.encode(gps_connection.read());                                      // Built in function in TinyGPS++ library to filter out the parameters from the NMEA standard format result obtained
      int a = (gps.location.isUpdated());
      Serial.println(a);
    }
    if (gps.location.isUpdated())                                             // Indicates whether the object’s value has been updated (not necessarily changed) since the last time you queried it.
    { 
      uint16_t ir, red;                                                       // Unsigned integer storing ir and red adc values
      uint8_t BPM,O2;                                                         
      //static uint32_t tsLastReport = millis();                                // The static keyword is used to create variables that are visible to only one function.
      uint32_t tsLastReport = millis();                                       // millis() - Returns the number of milliseconds passed since the Arduino board began running the current program. 
      sensor.begin();
      pox.begin();
      while(1)
      {        
        pox.update();                                                         // Used to get the updated values from sensor
        sensor.update();
        if(sensor.getRawValues(&ir, &red))                                    // Inbuilt function - getRawValues, used to fetch the IR and red light analog values
        {
          if ((millis() - tsLastReport) > 1000)                               // Checking at an interval of at least one second
          {
            Serial.println(l);
            BPM=round(pox.getHeartRate());                                    // Rounding the BPM to the nearest integer and getting the heart rate using inbuilt function:getHeartRate() 
            O2=pox.getSpO2();                                                 // Getting spo2 percentage level using inbuilt function:getSpO2()
            l=l+1;
            Serial.print("BPM: ");
            Serial.println(BPM);
            Serial.print("%SpO2: ");
            Serial.println(O2);
            if ((O2 == 0) || (BPM ==0)) 
            {
              l=0;
            }
            else if (l == 7)                                                  // Checking for upto 7 iterations for stables values of heart rate and spo2
            {
              l=0;
              break;
            }                                   
            tsLastReport = millis();                                    
          }
        }
      }
      float latitude = gps.location.lat();                                    // Reads the latitude and longitude as float values using inbuilt functions
      float longitude = gps.location.lng();
      delay(1000);
      gsm.listen();
      delay(1000);
      gsm.println("AT+CMGF=1");                                               // Used to select the operating mode of the GSM as SMS text mode
      delay(1000);
      gsm.println("AT+CSCS=\"GSM\"");                                         // This AT command selects the character set of the mobile equipment. Some possible values are "GSM", "HEX"."IRA", "PCDN", "UCS2","UTF-8" etc. Selected based on what our message contains. If it contains hexadecimal numbers hex is selected, here text is present in the message and hence GSM is used.
      delay(1000);
      gsm.println("AT+CMGS=\"+919978487678\"");                               // Marks the beginning of text, after which the content of the sms is conveyed
      delay(1000);
      gsm.print("Latitude: ");
      gsm.println(latitude,6);                                                // Latitude and longitude with 6 precision decimal places
      gsm.print("Longitude: ");
      gsm.println(longitude,6);
      gsm.print("BPM: ");
      gsm.println(BPM);
      gsm.print("%SpO2: ");
      gsm.println(O2);
      gsm.print("Temperature: ");
      gsm.println((analogRead(A1)* 0.48828125));                               // Finding temperature value using LM35; ADC - 10 bit 2^10 = 1024, 10mv - 1C, 5*1000/(1024*10); 1024 output form 5V
      delay(100);
      gsm.println((char)26);                                                   // #z used to end the CMGS command, char(26) represents the same
      delay(10000);                                                            // Large delay since message takes time to reach and also multiple iterations can be avoided which would otherwise occur on pressing the push button
      break;
    }
  }
}

void buzzon() 
{
  digitalWrite(7, HIGH);                                                        // Turning on the buzzer
}

void buzzoff() 
{
  digitalWrite(7, LOW);                                                          // Turning off the buzzer
}

// Similar procedure followed when push button is pressed

void help()
{
  gps_connection.listen();
  while (1)
  {
    while (gps_connection.available())
    {
      gps.encode(gps_connection.read()); 
      int a = (gps.location.isUpdated());
      Serial.println(a);
    }
    if (gps.location.isUpdated())
    { 
      uint16_t ir, red;                           
      uint8_t BPM,O2;
      static uint32_t tsLastReport = millis();
      sensor.begin();
      pox.begin();
      while(1)
      {        
        pox.update();  
        sensor.update();
        if(sensor.getRawValues(&ir, &red))
        {
          if ((millis() - tsLastReport) > 1000)
          {
            Serial.println(l);
            BPM=round(pox.getHeartRate());
            O2=pox.getSpO2();
            l=l+1;
            Serial.print("BPM: ");
            Serial.println(BPM);
            Serial.print("%SpO2: ");
            Serial.println(O2);
            if ((O2 == 0) || (BPM ==0)) 
            {
              l=0;
            }
            else if (l == 7)
            {
              l=0;
              break;
            }                                   
            tsLastReport = millis();                                    
          }
        }
      }
      float latitude = gps.location.lat();
      float longitude = gps.location.lng();
      delay(1000);
      gsm.listen();
      delay(1000);
      gsm.println("AT+CMGF=1");
      delay(1000);
      gsm.println("AT+CSCS=\"GSM\"");
      delay(1000);
      gsm.println("AT+CMGS=\"+919978487678\""); 
      delay(1000);
      gsm.println("Urgent help needed!!!");
      gsm.print("Latitude: ");
      gsm.println(latitude,6);
      gsm.print("Longitude: ");
      gsm.println(longitude,6);
      gsm.print("BPM: ");
      gsm.println(BPM);
      gsm.print("%SpO2: ");
      gsm.println(O2);
      gsm.print("Temperature: ");
      gsm.println((analogRead(A1)* 0.48828125));
      delay(100);
      gsm.println((char)26);
      delay(10000);
      break;
    }
  }
}

