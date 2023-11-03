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
int countEndProcess = 0;

// Variable Logging
int logCyble;
int logLiter;

int cybleSebelumnya;
int literSebelumnya;
int requestID;

// battrey
float battVolt;
const float multiplier = 0.1875F;
int persenBatt;
String statBatt;
bool hold = false;

bool countReset = 0;
int countTimeout = 0;

// Kondisi Proses
bool callEnd = 0;
bool callProcess = 0;
bool snyc = 1;
bool idle = 1;
bool process = 0;
int jumlahPesanan = 0;
int pembagi;
int setProgress;
int statMicro;

const String idDevice = "DASW240003";
const int LED2 = 2;

// Config Pin Valve
const int pinValve = 23;
const int powerValve = 13;

// Config Pin Cyble Sensor
const int LF_State = 25;
bool saveCount = 0;

// Cyble Counter
int literCounter = 0;
int CybleCounter_LF;
int SensorState_LF = 0;
int lastSensorState_LF = 0;

int CableState = 0;
int DIRState = 0;

void setup()
{
  EEPROM.begin(512);
  Serial.begin(19200);
  Serial2.begin(19200);
  Serial1.begin(19200, SERIAL_8N1, 14, 15);

  ads.begin();

  // kondisiButton
  pinMode(relayPower, OUTPUT);
  pinMode(pbPower, INPUT);
  pinMode(pbEmergency, INPUT);

  pinMode(pinValve, OUTPUT);
  pinMode(powerValve, OUTPUT);

  pinMode(LF_State, INPUT);
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
    requestID = doc["reqID"];
  }
  else
  {
    Serial.println("No 'LF' variable in eeprom.");
  }

  delay(1000);

  digitalWrite(powerValve, HIGH);
  digitalWrite(pinValve, LOW);

  delay(1000);
  Serial.println("System Ready");
  saveCount = 1;
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
  }
}

void dataUplink()
{
  location = String(latitude, 7) + "," + String(longitude, 7);

  StaticJsonDocument<256> dataUp;

  dataUp["nodeID"] = idDevice;
  dataUp["reqID"] = String(requestID);
  dataUp["loc"] = location;
  dataUp["Batt"] = String(statBatt) + "%";
  dataUp["status"] = status;
  dataUp["flow"] = String(flowSend);
  dataUp["logFlow"] = dataLog;
  serializeJson(dataUp, Serial2);
}

void antarMicroProses()
{
  StaticJsonDocument<300> microProses;
  microProses["reqID"] = requestID;
  microProses["req"] = jumlahPesanan;
  microProses["stan"] = CybleCounter_LF;
  microProses["count"] = literCounter;
  microProses["microStat"] = statMicro;
  serializeJson(microProses, Serial1);
}

void endProses()
{
  static uint32_t sendEndProcess = millis();
  // Serial.println("Stoping Valve");
  flowSend = String(CybleCounter_LF) + "," + String(literCounter);
  dataLog = String(cybleSebelumnya) + "," + String(literSebelumnya);
  statBatt = String(persenBatt);
  logCyble = CybleCounter_LF;
  logLiter = literCounter;

  if ((millis() - sendEndProcess) > (1500 + timeDelay))
  {
    sendEndProcess = millis();
    countEndProcess++;
    if (countEndProcess % 2 == 0)
    {
      dataUplink();
      Serial.println("Uplink");
    }
  }
  if (countEndProcess >= 15)
  {
    callEnd = 1;
    digitalWrite(pbPower, LOW);
    countEndProcess = 0;
    idle = 1;
    delay(2000);
    literCounter = 0;
    resetFunc();
  }
}

void prosesPengisian()
{
  static uint32_t timeReadLF = millis();
  static uint32_t timeProgress = millis();
  Serial.print("E: ");
  Serial.println(kondisiEmergency);
  Serial.print("S: ");
  Serial.println(SensorState_LF);

  if (!callEnd)
  {
    digitalWrite(pinValve, HIGH);
    digitalWrite(pbPower, HIGH);
    if (kondisiEmergency == 0 || (persenBatt >= 10 && persenBatt <= 23))
    {
      callEnd = 1;
      statMicro = 3;
      status = "4";
      antarMicroProses();
      delay(2000 + timeDelay);
      digitalWrite(pinValve, LOW);
      delay(13000);
    }
    if ((literCounter >= jumlahPesanan) && persenBatt >= 23)
    {
      callEnd = 1;
      statMicro = 3;
      status = "3";
      antarMicroProses();
      delay(2000 + timeDelay);
      digitalWrite(pinValve, LOW);
      delay(13000);
    }

    if ((millis() - timeReadLF) > 1000)
    {
      timeReadLF = millis();
      if (countInProcess <= 15)
      {
        countInProcess++;
      }
    }
    if (countInProcess >= 15)
    {
      saveCount = 1;
    }
    if ((millis() - timeProgress) > (120000 + timeDelay))
    {
      timeProgress = millis();
      flowSend = String(CybleCounter_LF) + "," + String(literCounter);
      statBatt = String(persenBatt);
      status = "6"; // Progress Air
      dataUplink();
    }
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
      int txInfo_item_transaction_id = txInfo_item["transaction"]["reqID"];      // 0, 0
      int txInfo_item_transaction_delay = txInfo_item["transaction"]["delay"];
      int txInfo_item_transaction_count = txInfo_item["transaction"]["startCnt"];

      if (String(txInfo_item_forID) == idDevice)
      {
        jumlahPesanan = txInfo_item_transaction_volume;
        requestID = txInfo_item_transaction_id;
        timeDelay = txInfo_item_transaction_delay;
        literCounter = txInfo_item_transaction_count;
        Serial.print("ID :");
        Serial.print(txInfo_item_forID);
        Serial.print(" volume : ");
        Serial.print(jumlahPesanan);
        Serial.print(" literCount : ");
        Serial.println(literCounter);
        Serial.print(" delay : ");
        Serial.println(timeDelay);

        if (txInfo_item_transaction_status == 1)
        {
          saveCount = 0;
          if (persenBatt >= 20)
          {
            countReset = 0;
            countTimeout = 0;
            digitalWrite(pinValve, HIGH);
            delay(1500 + timeDelay);
            kondisiEmergency = digitalRead(pbEmergency);
            delay(150 + timeDelay);
            if (kondisiEmergency == 0)
            {
              statMicro = 2;
              antarMicroProses();
              delay(100 + timeDelay);
              digitalWrite(pinValve, LOW);
            }
            else
            {
              idle = 0;
              cybleSebelumnya = logCyble;
              literSebelumnya = logLiter;
              flowSend = "0,0";
              dataLog = String(logCyble) + "," + String(logLiter);
              status = "2"; // Transaction Received
              statMicro = 1;
              antarMicroProses();
              delay(50 + timeDelay);
              dataUplink();
              delay(1000 + (timeDelay * 2));
              dataUplink();
              delay((1000 + timeDelay) + (timeDelay * 2));
              dataUplink();
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
          flowSend = "0,0";
          dataLog = String(logCyble) + "," + String(logLiter);
          Serial.print("Pesanan Masuk : ");
          Serial.println(jumlahPesanan);
          delay(150 + timeDelay);
          dataUplink();
          delay(150 + timeDelay);
          dataUplink();
          countReset = 1;
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
  }
}

void loop()
{

  // StaticJsonDocument<500> doc;
  // EepromStream eepromStream(0, 500);

  // doc["lf"] = 70;
  // doc["lat"] = latitude;
  // doc["long"] = longitude;
  // doc["lfB"] = logCyble;
  // doc["lB"] = logLiter;
  // doc["reqID"] = requestID;
  // serializeJson(doc, eepromStream);
  // eepromStream.flush();

  // Serial.print("Idle :");
  // Serial.println(idle);

  // Time Millis Count
  static uint32_t upIdleTime_now = millis();
  static uint32_t timeStart = millis();
  static uint32_t timeTimeout = millis();

  kondisiEmergency = digitalRead(pbEmergency);
  SensorState_LF = digitalRead(LF_State);
  bacaBattrey();

  if (SensorState_LF != lastSensorState_LF)
  {
    if (SensorState_LF == LOW)
    {
      if (saveCount)
      {
        CybleCounter_LF++;
      }
      if (idle == 0)
      {
        literCounter = literCounter + 100;
        statMicro = 1;
        antarMicroProses();
      }
      StaticJsonDocument<500> doc;
      EepromStream eepromStream(0, 500);

      doc["lf"] = CybleCounter_LF;
      doc["lat"] = latitude;
      doc["long"] = longitude;
      doc["lfB"] = logCyble;
      doc["lB"] = logLiter;
      doc["reqID"] = requestID;
      serializeJson(doc, eepromStream);
      eepromStream.flush();
    }
    lastSensorState_LF = SensorState_LF;
  }

  if (idle == 0)
  {
    prosesPengisian();
  }

  if (idle == 1)
  {

    SensorState_LF = digitalRead(LF_State);
    readGPS();
    digitalWrite(pbPower, LOW);
    digitalWrite(pinValve, LOW);
    dataDownlink();
    countInProcess = 0;
    if (countReset)
    {
      if ((millis() - timeTimeout) > 1000)
      {
        timeTimeout = millis();
        countTimeout++;
        Serial.println(countTimeout);
      }
      if (countTimeout >= 25)
      {
        resetFunc();
      }
    }

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

  if (callEnd)
  {
    endProses();
  }
}