#include <SoftwareSerial.h>
#include "TinyGPS.h"
#include <SD.h>


/*
 * BaseModule
 * 
 * v3.00 Updated to work with a Copernicus II's GPS readings. This version uses altitude
 * for its cutdown check, but the position (coordinates) is also available.
 * SD card logging capability added. Note that GPS connections changed for new software
 * serial pin assignments for XBee shield SD card compatibility.
 *
 * v1.10 Updated to avoid conflicts with other Serial messages and added support for
 * putting the XBee to sleep until near cutdown time.
 *
 * Brendan Batliner and Milan Shah
 * Illinois Mathematics and Science Academy - SIR Program
 *
*/

/* Notes on SD card integration - LN
  REF: http://arduino.cc/en/Main/ArduinoWirelessShield
  
  The Wireless shield hardwires Arduino pin 4 to the on-board
  SD card slot's SPI chip select (CS), so it can't be used as
  a Software Serial pin as in the TinyGPS example. Also
  The Arduino's MOSI, MISI and CLK pins (11,12 and 13) are used
  by the SD library and even if not used for SD CS, the default
  CS pin 10 must be programmed as an output.
  
  So, software serial can be assigned to 5 and 6 safely.
*/

#define XBEE_SLEEP 7
#define GPS_RX 5 //3 
#define GPS_TX 6 //4
#define LOG_FILE_NAME "baselog.txt"
const int chipSelectSD = 4;
boolean isLogging;

TinyGPS gps;
SoftwareSerial nss(GPS_TX, GPS_RX); // nss(Arduino Rx, Arduino Tx) = nss(GPS Tx, GPS Rx)
unsigned long startTime;
unsigned long endTime;
int flightTime;
double cutPercent = 0.9; //The percent of the flight that can go by before the BaseModule is
                      //authorized to cut the balloon.
float maxAltitude = 0;

void setup()
{
  Serial.begin(9600);
  Serial.println(maxAltitude);
  Serial.flush();
  nss.begin(4800);
  nss.flush();
  pinMode( 10, OUTPUT );
  pinMode(XBEE_SLEEP, OUTPUT);
  digitalWrite(XBEE_SLEEP, LOW); delay(1);  
  
  waitForTimeStart();
  
  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelectSD))
  {
    Serial.println("");
    Serial.println( "Base: SD Card failed, or not present" );
    Serial.println( "Base: No logging will be done." );
    Serial.println( "" );
    isLogging = false;
  } else
  {
    Serial.println( "Base: SD Card initialized" );
    // Check if dataFile can be created/opened and write header
    File dataFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
    if (dataFile)
    {
      dataFile.println("");
      dataFile.println("");
      dataFile.println("Cut Down Base Data Log");
      dataFile.println("");
      Serial.println("");
      Serial.println("Base: File opened, logging enabled.");
      Serial.println("");
      isLogging = true;
      dataFile.print("Max Flight Time: "); dataFile.println(flightTime);
      dataFile.print("Current Time: "); dataFile.println(startTime);
      dataFile.print("Authorized to cutdown at (time): "); dataFile.println(endTime);
      dataFile.print("Authorized to cutdown at (altitude [meters]): "); dataFile.println(maxAltitude);
      dataFile.print("Authorized to cutdown at (altitude [feet]): "); dataFile.println(maxAltitude*3.2804);
      dataFile.close();
    } else
    {
      Serial.println("");
      Serial.println( "Base: Can't open log file, logging disabled." );
      Serial.println( "" );
      isLogging = false;
    } 
  }

  //send xbee to sleep and move to loop function
  digitalWrite(XBEE_SLEEP, HIGH); delay(1); 
  
}

void waitForTimeStart()
{
  boolean timeReceived = false;
  while(!timeReceived)
  {
    if (Serial.available() > 0)
    {
      int incomingByte = Serial.read();
      if (incomingByte == 'X')
      {
        flightTime = Serial.parseInt();
        maxAltitude = Serial.parseFloat();
        maxAltitude /= 3.2804;
        
        startTime = millis();
        endTime = startTime + (cutPercent*flightTime*60*1000);
        timeReceived = true;
        Serial.println("Timer successfully started");
        Serial.print("Max Flight Time: "); Serial.println(flightTime);
        Serial.print("Current Time: "); Serial.println(startTime);
        Serial.print("Authorized to cutdown at (time): "); Serial.println(endTime);
        Serial.print("Authorized to cutdown at (altitude [meters]): "); Serial.println(maxAltitude);
        Serial.print("Authorized to cutdown at (altitude [feet]): "); Serial.println(maxAltitude*3.2804);
        Serial.flush();
        digitalWrite(XBEE_SLEEP, HIGH);
      }
    }
    delay(1);
  }  
}

void loop() 
{
  //if it is less than 30 seconds before endTime but not after endTime
  if (millis() >= endTime-30000 && millis() < endTime) 
  {
    //turn the xbee on
    digitalWrite(XBEE_SLEEP, LOW);
    delay(30000);
  }
  //if endTime has arrived
  else if (millis() >= endTime) 
  { 
    if (isLogging)
    {
      File dataFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
      if ( dataFile )
      {
        dataFile.print( millis() ); dataFile.println("*** Remote Cutdown at time initiated ***");
        dataFile.close();
      }
    }
    cutdown();
  }
  //else if the timer has not gone off yet, check the GPS
  else
  {
    while (nss.available() > 0)
    {
      int c = nss.read();
      if (gps.encode(c))
      {
        digitalWrite(XBEE_SLEEP, LOW); delay(1); //wake the xbee up
  
        //get data from the gps object
        long alt = gps.altitude()/100; //altitude() returns in centimeters. all of the conversions were previously done in meters. dividing by 100 converts cm's to m's.
        long lat, lon;
        gps.get_position(&lat, &lon);
        
        //Dr. Lou, these are the print statements that need to be converted into SD card prints. Also, the next if statement (with cutdown() )
        //will need to be changed in order to prevent the altitude check from accidentally cutting down the balloon. If you have any questions,
        //feel free to email us.
        //print all of the data, tab delimited
        //Serial.print(alt); Serial.print("\t");
        delay(1);
        Serial.print(lat); Serial.print("\t");
        Serial.print(lon); Serial.print("\t");
        Serial.println(alt);
        Serial.flush();
        if ( isLogging )
        {
          File dataFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
          
          if (dataFile)
          {
            dataFile.print(millis()); dataFile.print("\t");
            dataFile.print(lat); dataFile.print("\t");
            dataFile.print(lon); dataFile.print("\t");
            dataFile.println(alt);
            dataFile.close();
           }
       }
        //check if altitude is greater than maximum altitude
        if ((float)alt > (float)maxAltitude)
        {
          File dataFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
          if (dataFile)
          {
            dataFile.print( millis() ); dataFile.println("*** Remote cutdown at altitude initiated ***");
            dataFile.close();
          }
          //cutdown(); // Actual cutdown by altitude is suppressed for now.
        }
        
        //put the xbee back to sleep
        digitalWrite(XBEE_SLEEP, HIGH); delay(1); 
        
        //break out of the GPS's while loop so the timer can be checked again
        break;
      }
    }
    delay(20);
  }
  delay(20);
}

void cutdown()
{
  //wake up the xbee
  digitalWrite(XBEE_SLEEP, LOW); delay(1);
  //send 10 'c's, with a delay of 20ms between them
  for (int i = 0; i < 10; i++)
  {
    Serial.print('C');
    Serial.flush();
    delay(20);
  }
  
  //put the xbee back to sleep
  digitalWrite(XBEE_SLEEP, HIGH); delay(1);
}
