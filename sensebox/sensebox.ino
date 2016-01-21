#include <SPI.h>
#include <Ethernet.h>
/*
 * Zusätzliche Sensorbibliotheken, -Variablen etc im Folgenden einfügen.
 */
#include <Wire.h>
#include <HDC100X.h>
#include <Makerblog_TSL45315.h>
#include "BMP280.h"
 
//SenseBox ID
#define SENSEBOX_ID "56962f4db3de1fe00525c4dd"

//Sensor IDs

#define I2C_ADDR 0x38
#define IT_1   0x1 //1T

HDC100X HDC1(0x43);
BMP280 bmp;
Makerblog_TSL45315 luxsensor = Makerblog_TSL45315(TSL45315_TIME_M4);

uint32_t lux;


//Ethernet-Parameter
char server[] = "www.opensensemap.org";
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
// Diese IP Adresse nutzen falls DHCP nicht möglich
IPAddress myIP(192, 168, 0, 42);
EthernetClient client;

//Messparameter
int postInterval = 10000; //Uploadintervall in Millisekunden
long oldTime = 0;


void setup()
{
  Serial.begin(9600); 
  Serial.print("Starting network...");
  //Ethernet Verbindung mit DHCP ausführen..
  if (Ethernet.begin(mac) == 0) 
  {
    Serial.println("DHCP failed!");
    //Falls DHCP fehltschlägt, mit manueller IP versuchen
    Ethernet.begin(mac, myIP);
  }
  Serial.println("done!");
  delay(1000);

  HDC1.begin(HDC100X_TEMP_HUMI,HDC100X_14BIT,HDC100X_14BIT,DISABLE);
  
  luxsensor.begin();
  bmp.begin();
  bmp.setOversampling(4);
  
  Wire.begin();
  Wire.beginTransmission(I2C_ADDR);
  Wire.write((IT_1<<2) | 0x02);
  Wire.endTransmission();
  delay(500);
  
  Serial.println("Starting loop.");
}

void loop()
{
  //Upload der Daten mit konstanter Frequenz
  if (millis() - oldTime >= postInterval)
  {
    oldTime = millis();
    /*
     * Hier Sensoren auslesen und nacheinerander über postFloatValue(...) hochladen. Beispiel:
     * 
     * float temperature = sensor.readTemperature();
     * postFloatValue(temperature, 1, temperatureSensorID);
     */
     float humi = HDC1.getHumi();
  Serial.print("Luftfeuchte:         ");Serial.print(humi);Serial.println(" %");
  delay(200);

  float temp = HDC1.getTemp();
  Serial.print("Temperature:         ");Serial.print(temp);Serial.println(" C");
  delay(200);
  
  double T,P;
  char result = bmp.startMeasurment();
  delay(result);
  bmp.getTemperatureAndPressure(T,P);
  Serial.print("Luftdruck:           ");Serial.print(P); Serial.println(" hPa");
  Serial.print("Temperature #2:      ");Serial.print(T); Serial.println(" C");
  delay(200);

  lux = luxsensor.readLux();
  float flux = (float)lux;
  Serial.print("Beleuchtungsstaerke: ");
  Serial.print(flux, DEC);
  Serial.println(" lx");
  
  byte msb=0, lsb=0;
  uint16_t uv;

  Wire.requestFrom(I2C_ADDR+1, 1); //MSB
  delay(1);
  if(Wire.available())
    msb = Wire.read();

  Wire.requestFrom(I2C_ADDR+0, 1); //LSB
  delay(1);
  if(Wire.available())
    lsb = Wire.read();

  uv = (msb<<8) | lsb;
  uv = uv * 5.625;
  float fuv = uv * 5.625;
  Serial.print("UV-Strahlung:        ");Serial.print(fuv);Serial.println(" uW/cm2"); //output in steps (16bit)

  postFloatValue(humi,2,"hdc1_humidity_pct");
  postFloatValue(temp,2,"hdc1_temperature_c");
  postFloatValue(P,2,"bmp280_pressure_hpa");
  postFloatValue(T,2,"bmp280_temperature_c");
  postFloatValue(flux,0,"tsl45315_illumination_lux");
  postFloatValue(fuv,2,"tsl45315_uvirradiation_uwpcm2");
  
  Serial.println();      
  }
}

void postFloatValue(float measurement, int digits, String sensorId)
{ 
  //Float zu String konvertieren
  char obs[10]; 
  dtostrf(measurement, 5, digits, obs);
  //Json erstellen
  String jsonValue = "{\"value\":"; 
  jsonValue += obs; 
  jsonValue += "}";  
  //Mit OSeM Server verbinden und POST Operation durchführen
  Serial.println("-------------------------------------"); 
  Serial.print("Connectingto OSeM Server..."); 
  if (client.connect(server, 8000)) 
  {
    Serial.println("connected!");
    Serial.println("-------------------------------------");     
    //HTTP Header aufbauen
    client.print("POST /boxes/");client.print(SENSEBOX_ID);client.print("/");client.print(sensorId);client.println(" HTTP/1.1");
    client.println("Host: www.opensensemap.org"); 
    client.println("Content-Type: application/json"); 
    client.println("Connection: close");  
    client.print("Content-Length: ");client.println(jsonValue.length()); 
    client.println(); 
    //Daten senden
    client.println(jsonValue);
  }else 
  {
    Serial.println("failed!");
    Serial.println("-------------------------------------"); 
  }
  //Antwort von Server im seriellen Monitor anzeigen
  waitForServerResponse();
}

void waitForServerResponse()
{ 
  //Ankommende Bytes ausgeben
  boolean repeat = true; 
  do{ 
    if (client.available()) 
    { 
      char c = client.read();
      Serial.print(c); 
    } 
    //Verbindung beenden 
    if (!client.connected()) 
    {
      Serial.println();
      Serial.println("--------------"); 
      Serial.println("Disconnecting.");
      Serial.println("--------------"); 
      client.stop(); 
      repeat = false; 
    } 
  }while (repeat);
}
