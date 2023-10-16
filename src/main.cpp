#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include <StreamUtils.h>

#include <Adafruit_ADS1X15.h>
Adafruit_ADS1115 ads;

// kondisi Button
const int relayPower = 32;
const int pbPower = 27;
const int pbEmergency = 33;
int kondisiEmergency;
int kondisiPower;
int countInProcess = 0;

// millis time
const unsigned long periodUp_idle = 10000;
const unsigned long periodBatt = 1500;
int timeDelay;

// Konfirmasi Data Masuk
bool confirmTransaction = false;
bool confirmConfig = false;

double latitude;
double longitude;

// Variable Untuk Uplink
String location = "";
String status = "";
String flow = "";
String flowSend = "";
String dataLog = "";

// Variable Logging
int logCyble;
int logLiter;

int cybleSebelumnya;
int literSebelumnya;

// battrey
float battVolt;
const float multiplier = 0.1875F;
int persenBatt;
String statBatt;
bool hold = false;

const int numReadings = 10;
int readings[numReadings]; // the readings from the analog input
int readIndex = 0;         // the index of the current reading
int total = 0;             // the running total
int average = 0;

// Kondisi Proses
bool snyc = 1;
bool idle = 1;
bool process = 0;
int jumlahPesanan = 0;
int pembagi;
int setProgress;
int statMicro;

const String idDevice = "DASW240002";
const int LED2 = 2;

// Config Pin Valve
const int pinValve = 23;
const int powerValve = 13;

// Config Pin Lora
// const int configLora = 5;
// const int pinConfig = 4;

// Config Pin Cyble Sensor
const int LF_State = 25;
const int HF_State = 26;

// Cyble Counter
int literCounter = 0;
int CybleCounter_LF;
int SensorState_LF = 0;
int lastSensorState_LF = 0;

int CybleCounter_HF = 0;
int SensorState_HF = 0;
int lastSensorState_HF = 0;

int CableState = 0;
int DIRState = 0;

void setCounter(int nilai)
{
  Serial.println("Set LF");
  StaticJsonDocument<500> doc;
  EepromStream eepromStream(0, 500);

  // EepromStream eepromStream(0, 30);
  doc["lf"] = nilai;
  doc["lat"] = 0;
  doc["long"] = 0;
  doc["lfB"] = nilai;
  doc["lB"] = 0;
  serializeJson(doc, eepromStream);
  // EEPROM.commit();
  eepromStream.flush();
}

void setup()
{
  EEPROM.begin(512);
  Serial.begin(19200);
  Serial2.begin(19200);
  Serial1.begin(19200, SERIAL_8N1, 14, 15);
  // Wire.begin();
  // customKeypad.begin();
  ads.begin();

  // kondisiButton
  pinMode(relayPower, OUTPUT);
  pinMode(pbPower, INPUT);
  pinMode(pbEmergency, INPUT);

  pinMode(pinValve, OUTPUT);
  pinMode(powerValve, OUTPUT);

  // pinMode(configLora, OUTPUT);
  // pinMode(pinConfig, OUTPUT);

  // digitalWrite(configLora, LOW);
  // digitalWrite(pinConfig, LOW);

  pinMode(LF_State, INPUT);
  pinMode(HF_State, INPUT);
  StaticJsonDocument<500> doc;
  EepromStream eepromStream(0, 500);
  deserializeJson(doc, eepromStream);

  digitalWrite(pinValve, LOW);

  if (doc["lf"])
  {
    Serial.println("Loaded doc's LF value.");
    CybleCounter_LF = doc["lf"];
    latitude = doc["lat"];
    longitude = doc["long"];
    logCyble = CybleCounter_LF;
    logLiter = doc["lB"];
  }
  else
  {
    Serial.println("No 'LF' variable in eeprom.");
  }

  delay(1000);

  // setCounter(109);
  // digitalWrite(relayPower, HIGH);
  // digitalWrite(LED2, HIGH);
  digitalWrite(powerValve, HIGH);
  digitalWrite(pinValve, LOW);

  delay(1000);
  Serial.println("System Ready");
}

void (*resetFunc)(void) = 0;

void readGPS()
{
  if (Serial1.available())
  {
    StaticJsonDocument<150> datagps;
    DeserializationError error = deserializeJson(datagps, Serial1);

    if (error)
    {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }

    double latitudeGPS = datagps["lat"];   // "0000"
    double longitudeGPS = datagps["long"]; // "0000"
    latitude = latitudeGPS;
    longitude = longitudeGPS;

    // StaticJsonDocument<500> doc;
    // EepromStream eepromStream(0, 500);

    // doc["lf"] = CybleCounter_LF;
    // doc["lat"] = latitude;
    // doc["long"] = longitude;
    // doc["lfB"] = logCyble;
    // doc["lB"] = logLiter;
    // eepromStream.flush();
    // serializeJson(doc, eepromStream);
  }
}

void dataUplink()
{
  location = String(latitude, 7) + "," + String(longitude, 7);
  // flowSend = String(CybleCounter_LF) + "," + String(literCounter);
  // dataLog = String(logCyble) + "," + String(logLiter);

  StaticJsonDocument<256> dataUp;

  dataUp["nodeID"] = idDevice;
  dataUp["loc"] = location;
  dataUp["Batt"] = String(statBatt) + "%";
  dataUp["status"] = status;
  dataUp["flow"] = String(flowSend);
  dataUp["logFlow"] = dataLog;
  serializeJson(dataUp, Serial2);
}

void antarMicroProses()
{
  StaticJsonDocument<250> microProses;
  microProses["req"] = jumlahPesanan;
  microProses["stan"] = CybleCounter_LF;
  microProses["count"] = literCounter;
  microProses["microStat"] = statMicro;
  // microProses["batt"] = persenBatt;
  serializeJson(microProses, Serial1);
}

void endProses()
{
  digitalWrite(pinValve, LOW);
  Serial.println("Stoping Valve");
  status = "3";
  flowSend = String(CybleCounter_LF) + "," + String(literCounter);
  dataLog = String(cybleSebelumnya) + "," + String(literSebelumnya);
  statBatt = String(persenBatt);
  logCyble = CybleCounter_LF;
  logLiter = literCounter;
  Serial.println("Stoping Process");
  dataUplink();
  Serial.println("Uplink");
  delay(500);
  Serial.println("Stoping Process");
  delay(1000);
  dataUplink();
  delay(1500);
  dataUplink();
  delay(15000);
  digitalWrite(pbPower, LOW);
  idle = 1;
  delay(2000);
  resetFunc();
}

void prosesPengisian()
{

  digitalWrite(pinValve, HIGH);
  digitalWrite(pbPower, HIGH);
  Serial.println(CybleCounter_LF);

  // Serial.print("Set : ");
  // Serial.println(setProgress);

  // if (literCounter == setProgress)
  // {
  //   flowSend = String(CybleCounter_LF) + "," + String(literCounter);
  //   statBatt = String(persenBatt);
  //   status = "6"; // Progress Air
  //   dataUplink();
  //   setProgress = setProgress + pembagi;
  //   // delay(500);
  //   dataUplink();
  //   Serial.println("Masuk Counting");
  //   dataUplink();
  //   Serial.println("Uplink");
  //   dataUplink();
  //   Serial.println("Uplink");
  //   dataUplink();
  // }
  if (kondisiEmergency == 0 || (persenBatt >= 10 && persenBatt <= 23))
  {
    statMicro = 3;
    antarMicroProses();
    delay(2500);
    endProses();
  }
  if ((literCounter >= jumlahPesanan) && persenBatt >= 23)
  {
    statMicro = 3;
    antarMicroProses();
    delay(2500);
    endProses();
  }
}

void dataDownlink()
{
  // if ((Serial2.available() > 0) && idle == true && persenBatt >= 20)
  if ((Serial2.available() > 0) && idle == true)
  {
    StaticJsonDocument<512> list;
    DeserializationError error = deserializeJson(list, Serial2);
    if (error)
    {
      Serial.print("deserializeJson() failed Downlink: ");
      Serial.println(error.c_str());
      return;
    }

    // Serial.println(Serial2.read());

    for (JsonObject txInfo_item : list["downlink"].as<JsonArray>())
    {
      const char *txInfo_item_forID = txInfo_item["ID"];                         // "DASWMETER24VL0001", "DASWMETER24VL0001"
      int txInfo_item_transaction_status = txInfo_item["transaction"]["status"]; // 1, 1
      int txInfo_item_transaction_volume = txInfo_item["transaction"]["volume"]; // 0, 0
      int txInfo_item_transaction_delay = txInfo_item["transaction"]["delay"];

      if (String(txInfo_item_forID) == idDevice)
      {
        jumlahPesanan = txInfo_item_transaction_volume;
        timeDelay = txInfo_item_transaction_delay;
        Serial.print("ID :");
        Serial.print(txInfo_item_forID);
        Serial.print(" volume : ");
        Serial.println(jumlahPesanan);

        if (txInfo_item_transaction_status == 1)
        {
          if (persenBatt >= 20)
          {
            digitalWrite(pinValve, HIGH);
            delay(1500);
            kondisiEmergency = digitalRead(pbEmergency);
            delay(150);
            Serial.print("Emergency 0 ? ");
            Serial.println(kondisiEmergency);
            if (kondisiEmergency == 0)
            {
              Serial.println("Emergency BOS");
              statMicro = 2;
              antarMicroProses();
              delay(100);
              digitalWrite(pinValve, LOW);
            }
            else
            {
              cybleSebelumnya = logCyble;
              literSebelumnya = logLiter;
              status = "2"; // Transaction Received
              statMicro = 1;
              antarMicroProses();
              delay(50 + timeDelay);
              dataUplink();
              delay(70 + timeDelay);
              dataUplink();
              idle = 0;
            }
          }
        }
        if (txInfo_item_transaction_status == 2)
        {
          antarMicroProses();
          delay(150);
          status = "2"; // Transaction Received
          cybleSebelumnya = logCyble;
          literSebelumnya = logLiter;
          Serial.print("Pesanan Masuk : ");
          Serial.println(jumlahPesanan);
          delay(150 + timeDelay);
          dataUplink();
          delay(150 + timeDelay);
          dataUplink();
        }
      }
    }
  }
}

void bacaBattrey()
{
  static uint32_t battTime_now = millis();
  if ((millis() - battTime_now) > periodBatt)
  {
    battTime_now = millis();
    int16_t adc0, adc1, adc2, adc3;

    adc0 = ads.readADC_SingleEnded(0);
    adc1 = ads.readADC_SingleEnded(1);
    adc2 = ads.readADC_SingleEnded(2);
    adc3 = ads.readADC_SingleEnded(3);

    battVolt = (5.7676 * (adc0 * multiplier / 1000)) - 0.0403;
    persenBatt = (50 * (battVolt - 24));

    // battVolt = (6.5078 * (adc0 * multiplier / 1000)) + 0.0321;
    // persenBatt = (25 * (battVolt - 22));
    if (persenBatt > 100)
    {
      persenBatt = 100;
    }
    if (persenBatt < 0)
    {
      persenBatt = 0;
    }
    statBatt = String(persenBatt);
    Serial.print("Batt: ");
    Serial.println(persenBatt);
  }
}

void loop()
{

  // StaticJsonDocument<500> doc;
  // EepromStream eepromStream(0, 500);

  // doc["lf"] = 10;
  // doc["lat"] = latitude;
  // doc["long"] = longitude;
  // doc["lfB"] = logCyble;
  // doc["lB"] = logLiter;
  // serializeJson(doc, eepromStream);
  // eepromStream.flush();

  // doc["lf"] = 0;
  // doc["lat"] = "0000000";
  // doc["long"] = "000000";
  // doc["lfB"] = "0";
  // doc["lB"] = "0";
  // serializeJson(doc, eepromStream);
  // eepromStream.flush();

  // Serial.print("Idle :");
  // Serial.println(idle);

  // Time Millis Count
  static uint32_t upIdleTime_now = millis();
  static uint32_t timeProgress = millis();
  static uint32_t timeStart = millis();
  static uint32_t timeReadLF = millis();

  bacaBattrey();
  if (SensorState_LF != lastSensorState_LF)
  {
    if (SensorState_LF == LOW)
    {
      CybleCounter_LF++;

      StaticJsonDocument<500> doc;
      EepromStream eepromStream(0, 500);

      doc["lf"] = CybleCounter_LF;
      doc["lat"] = latitude;
      doc["long"] = longitude;
      doc["lfB"] = logCyble;
      doc["lB"] = logLiter;
      serializeJson(doc, eepromStream);
      eepromStream.flush();

      if (idle == 0)
      {
        literCounter = literCounter + 100;
        statMicro = 1;
        antarMicroProses();
        Serial.print("count : ");
        Serial.println(literCounter);
      }
    }
    lastSensorState_LF = SensorState_LF;
  }

  if (idle == 1)
  {
    SensorState_LF = digitalRead(LF_State);
    readGPS();
    digitalWrite(pbPower, LOW);
    // Serial.print("Batt :");
    // Serial.println(persenBatt);
    dataDownlink();
    literCounter = 0;
    setProgress = 0;
    countInProcess = 0;

    if ((millis() - upIdleTime_now) > periodUp_idle)
    {
      upIdleTime_now = millis();
      flowSend = "0,0";
      dataLog = String(logCyble) + "," + String(logLiter);
      status = "1"; // Siap Digunakan
      antarMicroProses();
      Serial.println("Idle");
      dataUplink();
      Serial.println(logLiter);
    }
  }

  if (idle == 0)
  {
    kondisiEmergency = digitalRead(pbEmergency);
    Serial.print("E : ");
    Serial.println(kondisiEmergency);
    if ((millis() - timeReadLF) > 1000)
    {
      timeReadLF = millis();
      if (countInProcess <= 15)
      {
        countInProcess++;
        // Serial.print("count :");
        // Serial.println(countInProcess);
      }
    }
    if (countInProcess >= 15)
    {
      // Serial.println("Baca Sensor");
      SensorState_LF = digitalRead(LF_State);
    }
    prosesPengisian();
    if ((millis() - timeProgress) > (120000 + timeDelay))
    {
      timeProgress = millis();
      flowSend = String(CybleCounter_LF) + "," + String(literCounter);
      statBatt = String(persenBatt);
      status = "6"; // Progress Air
      dataUplink();
    }
  }
  else
  {
    digitalWrite(pinValve, LOW);
  }
}