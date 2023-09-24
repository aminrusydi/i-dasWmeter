#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include <StreamUtils.h>

#include <Adafruit_ADS1X15.h>
Adafruit_ADS1115 ads;

const unsigned long periodUp_idle = 10000;
unsigned long upIdleTime_now = 0;

const unsigned long periodValve = 1000;
unsigned long valveTime_now = 0;

// kondisi Button
const int relayPower = 32;
const int pbPower = 27;
const int pbEmergency = 33;
int kondisiEmergency;
int kondisiPower;

// Konfirmasi Data Masuk
bool confirmTransaction = false;
bool confirmConfig = false;

// SoftwareSerial Serial1(14, 15);

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
const unsigned long periodBatt = 1000;
unsigned long battTime_now = 0;
const unsigned long periodUpbatt = 2000;
unsigned long uptimebat = 0;

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

const String idDevice = "DASWMETER24VL0001";
const int LED2 = 2;

// Config Pin Valve
const int pinValve = 23;
const int powerValve = 13;

// Config Pin Lora
const int configLora = 5;
const int pinConfig = 4;

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

  pinMode(configLora, OUTPUT);
  pinMode(pinConfig, OUTPUT);

  digitalWrite(configLora, LOW);
  digitalWrite(pinConfig, LOW);

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

    double latitudee = datagps["lat"];   // "0000"
    double longitudee = datagps["long"]; // "0000"
    latitude = latitudee;
    longitude = longitudee;

    StaticJsonDocument<500> doc;
    EepromStream eepromStream(0, 500);

    doc["lf"] = CybleCounter_LF;
    doc["lat"] = latitude;
    doc["long"] = longitude;
    doc["lfB"] = logCyble;
    doc["lB"] = logLiter;
    eepromStream.flush();
    serializeJson(doc, eepromStream);
  }
}

void dataUplink()
{
  location = String(latitude, 6) + "," + String(longitude, 6);
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
  StaticJsonDocument<150> microProses;
  microProses["req"] = jumlahPesanan;
  microProses["stan"] = CybleCounter_LF;
  microProses["count"] = literCounter;
  microProses["microStat"] = statMicro;
  // microProses["batt"] = persenBatt;
  serializeJson(microProses, Serial1);
}

void prosesPengisian()
{

  digitalWrite(pinValve, HIGH);
  digitalWrite(pbPower, HIGH);
  Serial.println(CybleCounter_LF);
  Serial.print("Set : ");
  Serial.println(setProgress);

  if (literCounter == setProgress)
  {
    flowSend = String(CybleCounter_LF) + "," + String(literCounter);
    statBatt = String(persenBatt);
    status = "6"; // Progress Air
    dataUplink();
    setProgress = setProgress + pembagi;
    // delay(500);
    dataUplink();
    Serial.println("Masuk Counting");
    dataUplink();
    Serial.println("Uplink");
    dataUplink();
    Serial.println("Uplink");
    dataUplink();
  }
  if (literCounter = jumlahPesanan || kondisiEmergency == 0 || (persenBatt > 0 && persenBatt < 20) || (literCounter >= jumlahPesanan && persenBatt > 20))
  {
    digitalWrite(pinValve, LOW);
    status = "3";
    flowSend = String(CybleCounter_LF) + "," + String(literCounter);
    dataLog = String(cybleSebelumnya) + "," + String(literSebelumnya);
    statBatt = String(persenBatt);
    // Transaction Done
    logCyble = CybleCounter_LF;
    logLiter = literCounter;
    Serial.println("Stoping Process");
    dataUplink();
    Serial.println("Stoping Process");
    delay(5000);
    dataUplink();
    delay(5000);
    dataUplink();
    delay(5000);
    dataUplink();
    delay(15000);
    statMicro = 0;
    antarMicroProses();
    delay(2000);
    digitalWrite(pbPower, LOW);
    resetFunc();

    idle = 1;
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
      const char *txInfo_item_forID = txInfo_item["ID"];                         // "DASWMETER24VL0001", "DASWMETER24VL0002"
      const char *txInfo_item_time = txInfo_item["time"];                        // "2023-02-28T05:41:11.038813Z", ...
      int txInfo_item_transaction_status = txInfo_item["transaction"]["status"]; // 1, 1
      int txInfo_item_transaction_volume = txInfo_item["transaction"]["volume"]; // 0, 0

      if (String(txInfo_item_forID) == idDevice)
      {
        jumlahPesanan = txInfo_item_transaction_volume;
        Serial.print("ID :");
        Serial.print(txInfo_item_forID);
        Serial.print(" volume : ");
        Serial.println(jumlahPesanan);

        if (txInfo_item_transaction_status == 1)
        {
          if (jumlahPesanan >= 500)
          {
            pembagi = jumlahPesanan / 5;
            setProgress = pembagi;
            // Serial.println(pembagi);
          }
          cybleSebelumnya = logCyble;
          literSebelumnya = logLiter;
          status = "2"; // Transaction Received
          idle = 0;
          dataUplink();
          delay(500);
          dataUplink();
          statMicro = 1;
          antarMicroProses();
          delay(1000);
        }
        else if (txInfo_item_transaction_status == 2)
        {
          antarMicroProses();
          cybleSebelumnya = logCyble;
          literSebelumnya = logLiter;
          Serial.println(jumlahPesanan);
          status = "2"; // Transaction Received
          dataUplink();
        }
      }
    }
  }
}

void bacaBattrey()
{
  if (millis() >= battTime_now + periodBatt)
  {
    battTime_now += periodBatt;
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

  // doc["lf"] = 1221;
  // doc["lat"] = latitude;
  // doc["long"] = longitude;
  // doc["lfB"] = logCyble;
  // doc["lB"] = logLiter;
  // serializeJson(doc, eepromStream);

  // eepromStream.flush();
  // Serial.print("Idle :");
  // Serial.println(idle);

  SensorState_LF = digitalRead(LF_State);
  kondisiEmergency = digitalRead(pbEmergency);
  bacaBattrey();
  Serial.println(pembagi);

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
      }
    }
    lastSensorState_LF = SensorState_LF;
  }

  if (idle == 1)
  {
    readGPS();
    digitalWrite(pbPower, LOW);
    // Serial.print("Batt :");
    Serial.println(persenBatt);
    dataDownlink();
    literCounter = 0;
    setProgress = 0;

    if (millis() >= upIdleTime_now + periodUp_idle)
    {
      antarMicroProses();
      upIdleTime_now += periodUp_idle;
      flowSend = "0,0";
      dataLog = String(logCyble) + "," + String(logLiter);
      status = "1"; // Siap Digunakan
      dataUplink();
      Serial.println(logLiter);
    }
  }

  if (idle == 0)
  {
    // Serial.print("Batt :");
    Serial.println(persenBatt);
    // if (persenBatt == 0)
    // {
    //   // btrai dicabut (batrai drop)
    //   hold = true;
    //   statBatt = "disable";
    //   if (millis() >= uptimebat + periodUpbatt)
    //   {
    //     uptimebat += periodUpbatt;
    //     dataUplink();
    //     Serial.println(hold);
    //     dataUplink();
    //     Serial.println(hold);
    //     dataUplink();
    //   }
    // }
    // if (persenBatt > 20 && hold == true)
    // {
    //   statBatt = String(persenBatt);
    //   dataUplink();
    //   Serial.println(hold);
    //   dataUplink();
    //   Serial.println(hold);
    //   dataUplink();
    //   hold = false;
    // }
    prosesPengisian();
  }
  else
  {
    digitalWrite(pinValve, LOW);
  }
}