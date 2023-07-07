//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~MASTER CONFIG~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#define WIFI_SSID             "IMSFL"               //SSID of network to connect to
#define WIFI_PWD              "V2$2uDY#P4"          //Password of the network
#define WIFI_RESET_TIME       10                    //Time in minutes until restart if WiFi isn't connected

#define DB_SP_ADDRESS         172,17,11,129         //Special Projects internal Database
#define DB_ADDRESS            172,17,0,91           //Database server IP address
#define DB_USER               "klima"               //Database username
#define DB_PWD                "klima_2020#"         //Database password
#define DB_DEFAULT            "klimadaten"          //Default database

#define STATION_NAME          "Halle_E"                 //Name of this station
#define STATION_ID            11                    //ID of this station

#define TYPE_SENSOR           "DHT"                 //Use "BME" or "DHT" to choose between BME280 and DHT22 sensors

#define NUMPIXELS             8                     //Number of NeoPixel LEDs
#define COLOR_BRT             0.25                  //Brightness multiplier for NeoPixel LEDs (0.0 - 1.0) 
#define THRESHOLD_CO2_UPPER   700                   //If CO2 exceeds this value the NeoPixels will shine red
#define THRESHOLD_CO2_LOWER   600                   //If CO2 is lower than this value the NeoPixels will shine green
#define THRESHOLD_ERRORS      10                    //Number of -3 returns after which a restart occurs

#define ADDRESS_NTP           "0.de.pool.ntp.org"   //Address of NTP server to fetch date / time information from
#define CALIBRATION_BEGIN     3                     //Beginning of calibration time frame in hours
#define CALIBRATION_END       6                     //End of calibration time frame in hours

#define ENABLE_WARMUP         true                 //Allow MH-Z19B to warum up before entering the main loop?

#define OTA_NAME              "CO2_Halle_E"             //Station hostname for OTA


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
#include <Adafruit_BME280.h>
#include <stdio.h>
#include <NTPClient.h>
#include <ArduinoOTA.h>


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~PIN CONFIG~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#define PIN_MHZ19B_TX   D5
#define PIN_MHZ19B_RX   D6
#define PIN_LED         D7
#define PIN_DHT         D4


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~SENSOR, LED, WIFI CONFIG & INIT~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#define MHZ19B_DETECT_DELAY 1000
#define MHZ19B_WARMUP_DELAY 2000

Adafruit_BME280 bme;
DHT dht(PIN_DHT, DHT22);

SoftwareSerial mhzSerial(PIN_MHZ19B_TX, PIN_MHZ19B_RX);
ErriezMHZ19B mhz19b(&mhzSerial);
WiFiClient client;
Adafruit_NeoPixel pixels (NUMPIXELS, PIN_LED, NEO_GRB + NEO_KHZ800);

byte errors = 0;


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~NTP & CALIBRATION~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ADDRESS_NTP, 3600, 60000);


byte calibrationTimeFrame[2] = {CALIBRATION_BEGIN,  CALIBRATION_END};

bool isCalibrated = false;
bool calibrationValid = false;

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
IPAddress SP_DB_addr(DB_SP_ADDRESS);

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


bool dbConnected;


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

void mySQLTest(char query[])
{
  if (conn.connect(SP_DB_addr, 3306, dbUser, dbPassword, default_db))
  {
    Serial.println(F("Connected to Special Projects database."));
    cur.execute(query);
    cur.close();
    conn.close();
  }
  else
  {
    Serial.println(F("Error: Can't connect to Special Projects database."));
  }
}

void mySQL(char query[])
{
  if (conn.connect(server_addr, 3306, dbUser, dbPassword, default_db))
  {
    dbConnected = true;
    Serial.println(F("Connected to database."));
    cur.execute(query);
    cur.close();
    conn.close();
  }
  else
  {
    dbConnected = false;
    Serial.println(F("Error: Can't connect to database."));
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

  if (TYPE_SENSOR == "BME")
  {
    temperature = bme.readTemperature();
    humidity = bme.readHumidity();
  }
  else if (TYPE_SENSOR == "DHT")
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

  if (TYPE_SENSOR == "BME")
  {
    bme.begin(0x76);
  }
  else if (TYPE_SENSOR == "DHT")
  {
    dht.begin();
  }

  mhz19b.setAutoCalibration(false);
  Serial.print("\nAutomatic Baseline Calibration turned OFF.\n");

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

  timeClient.begin();

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
}

void loop()
{
  ArduinoOTA.handle();

  if (millis() % 60000 == 0)
  {
    //Calibration

    timeClient.update();
    byte currentHours = (byte)timeClient.getHours();

    if (currentHours >= calibrationTimeFrame[0] && currentHours <= calibrationTimeFrame[1])
    {
      if (!isCalibrated)
      {
        mhz19b.startZeroCalibration();
        isCalibrated = true;
        calibrationValid = true;
        Serial.print(F("\nBaseline calibraton done.\n\n"));
      }
    }
    else
    {
      isCalibrated = false;
      Serial.print(F("\nOutside of calibration timeframe.\n\n"));
    }


    //Get Measurements

    getData();


    //Upload measurements to database

    sprintf(statement, "INSERT INTO co2 (device, date_entry, co2, temperature, air_humidity, air_pressure) VALUES ('%d', now(), '%d', '%f', '%f', '%f')",
            id, CO2, temperature, humidity, pressure);
    Serial.println(statement);

    mySQL(statement);
    mySQLTest(statement);
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
          if (WiFi.status() != WL_CONNECTED)
          {
            setLED(COLOR_TURQUOISE);
          }
          else
            setLED(COLOR_LILA);
        }
        delay(300);
        pixels.clear();
        delay(300);
      }
    }
    
    if (CO2 > THRESHOLD_CO2_UPPER)
      setLED(COLOR_RED);
    else if (CO2 < THRESHOLD_CO2_LOWER)
      setLED(COLOR_GREEN);
    else
      setLED(COLOR_YELLOW);

  }
}
