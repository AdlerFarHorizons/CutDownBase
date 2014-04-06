#include <SoftwareSerial.h>
#include <math.h>
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
  
  TESTING:
  Can be tested without remote cutdown and remote programmer units:
  1. Remove XBee from XBee shield so Arduino USB I/O will work
  2. Provide input via a single monitor line entry emulating the
     remote inputs, for example:
     X90,100000,300,41.1000322,-87.9167100
     This yields: Remote cutdown time (deadman): 90 minutes
                  Cutdown altitude: 100,000 feet
                  Cutdown range: 300 miles
                  Launch Lat/Lon: Koerner Aviation
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
double maxAltitude = 0;
double maxRadius = 0;
double center_lat = 0;
double center_lon = 0;
const double RADIUS = 3963.1676; //radius of the earth in miles
boolean isCutdown = false; //boolean to store whether the BaseModule has sent a cutdown command or not

void setup()
{
  Serial.begin(9600);
  Serial.println(freeRam());
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
      dataFile.print("Max Flight Time (minutes): "); dataFile.println(flightTime);
      dataFile.print("Current Time (millis): "); dataFile.println(startTime);
      dataFile.print("Authorized to cutdown at (time): "); dataFile.println(endTime);
      dataFile.print("Authorized to cutdown at (altitude [meters]): "); dataFile.println(maxAltitude);
      dataFile.print("Authorized to cutdown at (altitude [feet]): "); dataFile.println(maxAltitude*3.2804); //convert to feet
      dataFile.print("Authorized to cutdown at "); dataFile.print(maxRadius); dataFile.print(" miles from "); dataFile.print(center_lat); dataFile.print(", "); dataFile.println(center_lon);
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
        maxAltitude /= 3.2804; //convert to meters
        maxRadius = Serial.parseFloat();
        center_lat = Serial.parseFloat();
        center_lon = Serial.parseFloat();
        
        startTime = millis();
        endTime = startTime + (cutPercent*flightTime*60*1000);
        timeReceived = true;
        Serial.println("Timer successfully started");
        Serial.print("Max Flight Time (minutes): "); Serial.println(flightTime);
        Serial.print("Current Time (millis): "); Serial.println(startTime);
        Serial.print("Authorized to cutdown at (time): "); Serial.println(endTime);
        Serial.print("Authorized to cutdown at (altitude [meters]): "); Serial.println(maxAltitude);
        Serial.print("Authorized to cutdown at (altitude [feet]): "); Serial.println(maxAltitude*3.2804); //convert to feet
        Serial.print("Authorized to cutdown at "); Serial.print(maxRadius); Serial.print("miles from "); Serial.print(center_lat); Serial.print(", "); Serial.println(center_lon);
        Serial.flush();
        digitalWrite(XBEE_SLEEP, HIGH);
      }
    }
    delay(1);
  }  
}

void loop() 
{
  //if endTime has arrived
  if (millis() >= endTime ) 
  { 
    if (!isCutdown && isLogging) //if logging is enabled, log some stuff
    {
      File dataFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
      if ( dataFile )
      {
        dataFile.print( millis() ); dataFile.println("*** Remote Cutdown at time initiated ***");
        dataFile.close();
      }
      isCutdown = true; // Run only once
    }
    cutdown();
  }
  //Check the GPS
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
        double scaledLat = lat / pow(10,6); //divide by 10^6
        double scaledLon = lon / pow(10,6); //divide by 10^6
        unsigned long date, time, age;
        gps.get_datetime(&date, &time, &age);

        //print all of the data, tab delimited
        //Serial.print(alt); Serial.print("\t");
        delay(1);
        Serial.print(millis()); Serial.print("\t");
        Serial.print(time); Serial.print("\t");
        Serial.print(scaledLat); Serial.print("\t");
        Serial.print(scaledLon); Serial.print("\t");
        Serial.println(alt);
        Serial.flush();
        if ( isLogging )
        {
          File dataFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
          
          if (dataFile)
          {
            dataFile.print(millis()); dataFile.print("\t");
            dataFile.print(time); dataFile.print("\t");
            dataFile.print(scaledLat); dataFile.print("\t");
            dataFile.print(scaledLon); dataFile.print("\t");
            dataFile.println(alt);
            dataFile.close();
           }
       }
        //check if altitude is greater than maximum altitude
        if ((double)alt > (double)maxAltitude && !isCutdown)
        {
          File dataFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
          if (dataFile)
          {
            dataFile.print( millis() ); dataFile.println("*** Remote cutdown at altitude initiated ***");
            dataFile.close();
          }
          //cutdown(); // Actual cutdown by altitude is suppressed for now.
          isCutdown = true; //run only once
        }
        
        double d = distanceBetweenTwoPoints(scaledLat, scaledLon, center_lat, center_lon);
        
        if (d > maxRadius)
        {
          //Serial.println("Cutdown by max radius of flight.");
          //cutdown(); // suppressed for now
          isCutdown = true; //run only once
        }
        
        //put the xbee back to sleep
        digitalWrite(XBEE_SLEEP, HIGH); delay(1); 
        
        //break out of the GPS's while loop so the timer can be checked again
        break;
      }
    }
    delay(40);
}

double distanceBetweenTwoPoints(double lat1, double lon1, double lat2, double lon2)
{
  double dLat = deg2rad(lat2-lat1);
  double dLon = deg2rad(lon2-lon1);
  double latOne = deg2rad(lat1);
  double latTwo = deg2rad(lat2);
  
  double a = sin(dLat/2.0)*sin(dLat/2.0) +
             sin(dLon/2.0)*sin(dLon/2.0) * cos(lat1)*cos(lat2);
  double c = 2.0 * atan2(sqrt(a), sqrt(1-a));
  double d_miles = RADIUS * c;
  return d_miles;
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

double deg2rad(double degree)
{
  return degree * (PI/180.0);
}

int freeRam() 
{
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}
