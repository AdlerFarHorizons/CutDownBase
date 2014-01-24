#include <SoftwareSerial.h>
#include "TinyGPS.h"


/*
 * BaseModule
 * 
 * Initial Baseline for GitHub tracking
 *
 * Previous history:
 *
 * v3.00 Updated to work with a Copernicus II's GPS readings. This version uses altitude
 * for its cutdown check, but the position (coordinates) is also available.
 *
 * v1.10 Updated to avoid conflicts with other Serial messages and added support for
 * putting the XBee to sleep until near cutdown time.
 *
 * Brendan Batliner and Milan Shah
 * Illinois Mathematics and Science Academy - SIR Program
 *
*/

#define XBEE_SLEEP 7
#define GPS_RX 3 
#define GPS_TX 4 

TinyGPS gps;
SoftwareSerial nss(GPS_TX, GPS_RX); // nss(Arduino Rx, Arduino Tx) = nss(GPS Tx, GPS Rx)
unsigned long startTime;
unsigned long endTime;
int flightTime;
double cutPercent = 0.9; //The percent of the flight that can go by before the BaseModule is
                      //authorized to cut the balloon.
float maxAltitude = 0;

void cutdown();

void setup()
{
  Serial.begin(9600);
  Serial.println(maxAltitude);
  Serial.flush();
  nss.begin(4800);
  nss.flush();
  pinMode(XBEE_SLEEP, OUTPUT);
  digitalWrite(XBEE_SLEEP, LOW);
  waitForTimeStart();
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
        Serial.println(flightTime);
        Serial.print("Current Time: "); Serial.println(startTime);
        Serial.print("Authorized to cutdown at (time): "); Serial.println(endTime);
        Serial.print("Authorized to cutdown at (altitude [meters]): "); Serial.println(maxAltitude);
        Serial.print("Authorized to cutdown at (altitude [feet]): "); Serial.println(maxAltitude*3.2804);
        Serial.flush();
        
        //send xbee to sleep and move to loop function
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
        digitalWrite(XBEE_SLEEP, LOW); //wake the xbee up
  
        //get data from the gps object
        long alt = gps.altitude();
        long lat, lon;
        gps.get_position(&lat, &lon);
        
        //Dr. Lou, these are the print statements that need to be converted into SD card prints. Also, the next if statement (with cutdown() )
        //will need to be changed in order to prevent the altitude check from accidentally cutting down the balloon. If you have any questions,
        //feel free to email us.
        //print all of the data, tab delimited
        Serial.print(alt); Serial.print("\t");
        Serial.print(lat); Serial.print("\t");
        Serial.print(lon); //Serial.print("\t");
        Serial.flush();
        
        //check if altitude is greater than maximum altitude
        if ((float)alt > (float)maxAltitude)
        {
          cutdown();
        }
        
        //put the xbee back to sleep
        digitalWrite(XBEE_SLEEP, HIGH); 
        
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
  digitalWrite(XBEE_SLEEP, LOW);
  
  //send 10 'c's, with a delay of 20ms between them
  for (int i = 0; i < 10; i++)
  {
    Serial.print('C');
    Serial.flush();
    delay(20);
  }
  
  //put the xbee back to sleep
  digitalWrite(XBEE_SLEEP, HIGH);
}
