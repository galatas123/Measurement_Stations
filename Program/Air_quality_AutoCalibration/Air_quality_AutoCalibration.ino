//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~MASTER CONFIG~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#define WIFI_SSID             "IMSFL"              //SSID of network to connect to               IM_Flensburg  "" 
#define WIFI_PWD              "V2$2uDY#P4"         //Password of the network                      Ingram123@ "" 
#define WIFI_RESET_TIME       20                   //Time in minutes until restart if WiFi isn't connected

#define DB_ADDRESS            172,17,0,91           //Database server IP address
#define DB_USER               "xxxxx"               //Database username
#define DB_PWD                "xxxxxxxxxxx"         //Database password
#define DB_DEFAULT            "xxxxxxxxxx"          //Default database
#define STATION_NAME          "Halle_A"             //Name of this station
#define STATION_ID            6                     //ID of this station

#define TYPE_SENSOR           "DHT"                 //Use "BME" or "DHT" to choose between BME280 and DHT22 sensors

#define NUMPIXELS             8                     //Number of NeoPixel LEDs
#define COLOR_BRT             0.25                  //Brightness multiplier for NeoPixel LEDs (0.0 - 1.0) 
#define THRESHOLD_ERRORS      20                    //Number of -3 returns after which a restart occurs

#define ENABLE_WARMUP         false                 //Allow MH-Z19B to warum up before entering the main loop?

#define OTA_NAME              "Halle_A"             //Station hostname for OTA


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~LIBRARIES~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#include <DHT.h>
#include <DHT_U.h>
#include <ErriezMHZ19B.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#include <Adafruit_Sensor.h>
#include <stdio.h>
#include <ArduinoOTA.h>


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~PIN CONFIG~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#define PIN_MHZ19B_TX   D5
#define PIN_MHZ19B_RX   D6
#define PIN_LED         D7
#define PIN_DHT         D4


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~SENSOR, LED, WIFI CONFIG & INIT~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#define MHZ19B_DETECT_DELAY 1000
#define MHZ19B_WARMUP_DELAY 2000

DHT dht(PIN_DHT, DHT22);

SoftwareSerial mhzSerial(PIN_MHZ19B_TX, PIN_MHZ19B_RX);
ErriezMHZ19B mhz19b(&mhzSerial);
WiFiClient client;
Adafruit_NeoPixel pixels (NUMPIXELS, PIN_LED, NEO_GRB + NEO_KHZ800);

byte errors = 0;
byte dbErrors = 0;


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~MEASUREMENTS~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

int16_t CO2;
float temperature, humidity, pressure = 0.0f;


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~WIFI CONFIG~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

const char* ssid = WIFI_SSID;
const char* pass = WIFI_PWD;
const char* station = STATION_NAME;
const int id = STATION_ID;

unsigned long wifiDownTime;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~MYSQL CONFIG & INIT~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

IPAddress server_addr(DB_ADDRESS);

char dbUser[] = DB_USER;
char dbPassword[] = DB_PWD;
char default_db[] = DB_DEFAULT;

MySQL_Connection conn((Client *)&client);
MySQL_Cursor cur = MySQL_Cursor(&conn);

char statement[200];

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~COLORS~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#define COLOR_RED       0xFF, 0x00, 0x00
#define COLOR_GREEN     0x00, 0xFF, 0x00
#define COLOR_BLUE      0x00, 0x00, 0xFF
#define COLOR_YELLOW    0xFF, 0x82, 0x00
#define COLOR_TURQUOISE 0x30, 0xD5, 0xC8
#define COLOR_LILA      0xFF, 0x00, 0xFF
#define COLOR_WEISS     0xFF, 0xFF, 0xFF
#define OFF             0x00, 0x00, 0x00


bool dbConnected = true;


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~FUNCTIONS~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void setLED(byte r, byte g, byte b)
{
  pixels.clear();

  for (int i = 0; i < NUMPIXELS; i++)
  {
    pixels.setPixelColor(i, pixels.Color(r * COLOR_BRT, g * COLOR_BRT, b * COLOR_BRT));
    pixels.show();
  }
}


void mySQL(char query[])
{
  if (conn.connect(server_addr, 3306, dbUser, dbPassword, default_db))
  {
    dbErrors = 0;
    dbConnected = true;
    Serial.println(F("Connected to database."));
    cur.execute(query);
    cur.close();
    conn.close();
  }
  else
  {
    dbErrors ++;
    dbConnected = false;
    Serial.println(F("Error: Can't connect to database."));
  }
  if (dbErrors > THRESHOLD_ERRORS)
  {
    ESP.restart();
  }
}


int16_t readCo2()
{
  int16_t result;

  if (mhz19b.isReady())
  {
    result = mhz19b.readCO2();
  }

  return result;
}

void getData()
{
  CO2 = readCo2();

  if(CO2 == -3)
    errors++;
  else
    errors = 0;

  if(errors >= THRESHOLD_ERRORS)
    ESP.restart();

  if (TYPE_SENSOR == "DHT")
  {
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
  }
  else
  {
    temperature = 0.0f;
    humidity = 0.0f;
  }

  Serial.print("\nCO2:\t\t");
  Serial.print(CO2);
  Serial.print("ppm\nTemperature:\t");
  Serial.print(temperature);
  Serial.print("°C\nHumidity:\t");
  Serial.print(humidity);
  Serial.println("%");
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~SETUP & LOOP~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void setup()
{
  Serial.begin(115200);
  mhzSerial.begin(9600);
  pixels.begin();
  setLED(COLOR_BLUE);
  dht.begin();


  mhz19b.setAutoCalibration(true);

  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.println(F("Connecting..."));

    delay(2000);
  }

  Serial.print(F("\nIP Adress:\t"));
  Serial.println(WiFi.localIP());
  Serial.print(F("MAC Address:\t"));
  Serial.println(WiFi.macAddress());
  Serial.print("Station name:");
  Serial.println(station);
  delay(5000);

  //ARDUINO OTA BEGIN

  ArduinoOTA.setHostname(OTA_NAME);

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  //ARDUINO OTA END


  while (!mhz19b.detect())
  {
    Serial.println(F("Detecting MH-Z19B sensor..."));

    delay(2000);
  }


  if (ENABLE_WARMUP)
  {
    while (mhz19b.isWarmingUp())
    {
      Serial.println(F("MH-Z19B warming up..."));

      delay(2000);
    }
  }

  mhz19b.setRange2000ppm();

  Serial.print(F("\nRange:\t"));
  Serial.print(mhz19b.getRange());
  Serial.println(F("ppm"));

  if (conn.connect(server_addr, 3306, dbUser, dbPassword, default_db))
    dbConnected = true;

  setLED(OFF);
}

void loop()
{
  ArduinoOTA.handle();

  if (millis() % 60000 == 0)
  {
    //Get Measurements

    getData();


    //Upload measurements to database

    sprintf(statement, "INSERT INTO co2 (device, date_entry, co2, temperature, air_humidity, air_pressure) VALUES ('%d', now(), '%d', '%f', '%f', '%f')",
            id, CO2, temperature, humidity, pressure);
    Serial.println(statement);

    mySQL(statement);
  }

  if (millis() % 5000 == 0)
  {
    if(WiFi.status() != WL_CONNECTED)
    {
      Serial.print(F("\nWifi Connection lost ("));
      Serial.print(wifiDownTime / 1000);
      Serial.print(F(" s). Restart after "));
      Serial.print(WIFI_RESET_TIME);
      Serial.print(F(" m.\n"));
      
      if(millis() - wifiDownTime >= WIFI_RESET_TIME * 60 * 1000)
        ESP.restart();
    }
    else
    {
      wifiDownTime = millis();
    }
    
    if (!dbConnected || (WiFi.status() != WL_CONNECTED))
    {
      for (int i = 0; i < 4; i++)
      {
        if (!dbConnected)
        {
          setLED(COLOR_LILA);
        }
        else setLED(COLOR_TURQUOISE);
        delay(300);
        pixels.clear();
        delay(300);
      }
    }
    else
    {
      setLED(OFF);
    }
  }
}
