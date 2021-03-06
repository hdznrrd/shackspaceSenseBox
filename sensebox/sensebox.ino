/*
SenseBox Home - Citizen Sensingplatform
Version: 2.1
Date: 2015-09-09
Homepage: http://www.sensebox.de
Author: Jan Wirwahn, Institute for Geoinformatics, University of Muenster
Note: Sketch for SenseBox Home Kit
Email: support@sensebox.de 
*/

#include <Wire.h>
#include "HDC100X.h"
#include "BMP280.h"
#include <Makerblog_TSL45315.h>
#include <SPI.h>
#include <Ethernet.h>

//SenseBox ID
#define SENSEBOX_ID "56a0de932cb6e1e41040a68b"

//Sensor IDs
#define TEMPSENSOR_ID "56a0de932cb6e1e41040a691"
#define HUMISENSOR_ID "56a0de932cb6e1e41040a690"
#define PRESSURESENSOR_ID "56a0de932cb6e1e41040a68f"
#define LUXSENSOR_ID "56a0de932cb6e1e41040a68e"
#define UVSENSOR_ID "56a0de932cb6e1e41040a68d"

//Configure ethernet connection
IPAddress myIp(192, 168, 0, 42);
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
char server[] = "www.opensensemap.org";
EthernetClient client;

//Load sensors
Makerblog_TSL45315 TSL = Makerblog_TSL45315(TSL45315_TIME_M4);
HDC100X HDC(0x43);
BMP280 BMP;

//measurement variables
float temperature = 0;
float humidity = 0;
double tempBaro, pressure;
uint32_t lux;
uint16_t uv;
int messTyp;
#define UV_ADDR 0x38
#define IT_1   0x1

unsigned long oldTime = 0;
const unsigned long postingInterval = 60000;

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  // start the Ethernet connection:
  Serial.println("SenseBox Home software version 2.1");
  Serial.println();
  Serial.print("Starting ethernet connection...");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    Ethernet.begin(mac, myIp);
  }else Serial.println("done!");
  delay(1000);
  //Initialize sensors
  Serial.print("Initializing sensors...");
  Wire.begin();
  Wire.beginTransmission(UV_ADDR);
  Wire.write((IT_1<<2) | 0x02);
  Wire.endTransmission();
  delay(500);
  HDC.begin(HDC100X_TEMP_HUMI,HDC100X_14BIT,HDC100X_14BIT,DISABLE);
  TSL.begin();
  BMP.begin();
  BMP.setOversampling(4);
  Serial.println("done!");
  Serial.println("Starting loop.");
  temperature = HDC.getTemp();
}

void loop()
{
  // if there are incoming bytes available
  // from the server, read them and print them:
  if (client.available()) {
    char c = client.read();
    Serial.print(c);
    //Serial.write(c);
  }

  if (millis() - oldTime > postingInterval) {
    oldTime = millis();   
    //-----Pressure-----//
    Serial.println("Posting pressure");
    messTyp = 2;
    char result = BMP.startMeasurment();
    if(result!=0){
      delay(result);
      result = BMP.getTemperatureAndPressure(tempBaro,pressure);
      postObservation(pressure, PRESSURESENSOR_ID, SENSEBOX_ID); 
      //Serial.print("Temp_baro = ");Serial.println(tempBaro,2);
      //Serial.print("Pressure  = ");Serial.println(pressure,2);
    }
    delay(2000); 
    //-----Humidity-----//
    Serial.println("Posting humidity");
    messTyp = 2;
    humidity = HDC.getHumi();
    //Serial.print("Humidity = "); Serial.println(humidity);
    postObservation(humidity, HUMISENSOR_ID, SENSEBOX_ID); 
    delay(2000);
    //-----Temperature-----//
    Serial.println("Posting temperature");
    messTyp = 2;
    temperature = HDC.getTemp();
    //Serial.println(temperature,2);
    //Serial.print("Temperature = ");Serial.println(temperature);
    postObservation(temperature, TEMPSENSOR_ID, SENSEBOX_ID); 
    delay(2000);  
    //-----Lux-----//
    Serial.println("Posting illuminance");
    messTyp = 1;
    lux = TSL.readLux();
    //Serial.print("Illumi = "); Serial.println(lux);
    postObservation(lux, LUXSENSOR_ID, SENSEBOX_ID);
    delay(2000);
    //UV intensity
    messTyp = 1;
    uv = getUV();
    postObservation(uv, UVSENSOR_ID, SENSEBOX_ID);
  }
}

void postObservation(float measurement, String sensorId, String boxId){ 
  char obs[10]; 
  if (messTyp == 1) dtostrf(measurement, 5, 0, obs);
  else if (messTyp == 2) dtostrf(measurement, 5, 2, obs);
  Serial.println(obs); 
  //json must look like: {"value":"12.5"} 
  //post observation to: http://opensensemap.org:8000/boxes/boxId/sensorId
  Serial.println("connecting..."); 
  String value = "{\"value\":"; 
  value += obs; 
  value += "}";
  if (client.connect(server, 8000)) 
  {
    Serial.println("connected"); 
    // Make a HTTP Post request: 
    client.print("POST /boxes/"); 
    client.print(boxId);
    client.print("/"); 
    client.print(sensorId); 
    client.println(" HTTP/1.1"); 
    // Send the required header parameters 
    client.println("Host:opensensemap.org"); 
    client.println("Content-Type: application/json"); 
    client.println("Connection: close");  
    client.print("Content-Length: "); 
    client.println(value.length()); 
    client.println(); 
    // Send the data
    client.print(value); 
    client.println(); 
  } 
  waitForResponse();
}

void waitForResponse()
{ 
  // if there are incoming bytes available 
  // from the server, read them and print them: 
  boolean repeat = true; 
  do{ 
    if (client.available()) 
    { 
      char c = client.read();
      Serial.print(c); 
    } 
    // if the servers disconnected, stop the client: 
    if (!client.connected()) 
    {
      Serial.println();
      Serial.println("disconnecting."); 
      client.stop(); 
      repeat = false; 
    } 
  }
  while (repeat);
}

uint16_t getUV(){
  byte msb=0, lsb=0;
  uint16_t uvValue;

  Wire.requestFrom(UV_ADDR+1, 1); //MSB
  delay(1);
  if(Wire.available()) msb = Wire.read();

  Wire.requestFrom(UV_ADDR+0, 1); //LSB
  delay(1);
  if(Wire.available()) lsb = Wire.read();

  uvValue = (msb<<8) | lsb;

  return uvValue*5;
}
