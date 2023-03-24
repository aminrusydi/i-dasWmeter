#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

// #include <Keypad_MC17.h>
// #include <Keypad.h>
// #include <ctype.h>
#include <Wire.h>
#include <SPI.h>

#include <ArduinoJson.h>
#include <StreamUtils.h>
#include <TinyGPSPlus.h>

#include <Adafruit_ADS1X15.h>
Adafruit_ADS1115 ads;

const unsigned long periodUp_idle = 10000;
unsigned long upIdleTime_now = 0;

const unsigned long periodValve = 1000;
unsigned long valveTime_now = 0;

LiquidCrystal_I2C lcd(0x27, 20, 4);

// kondisi Button
const int relayPower = 32;
const int pbPower = 27;
const int pbEmergency = 33;
int kondisiPower;

// Konfirmasi Data Masuk
bool confirmTransaction = false;
bool confirmConfig = false;

// SoftwareSerial Serial1(14, 15);
TinyGPSPlus gps;
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

// Kondisi Proses
bool snyc = 1;
bool idle = 1;
bool process = 0;
int jumlahPesanan = 0;

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

// // Keypad Configuration
// #define I2CADDR 0x20

// const byte ROWS = 5; // four rows
// const byte COLS = 4; // three columns
// // Define the keymaps.  The blank spot (lower left) is the space character.
// char hexaKeys[ROWS][COLS] = {
//     {'A', 'B', 'C', 'F'},
//     {'1', '2', '3', 'U'},
//     {'4', '5', '6', 'D'},
//     {'7', '8', '9', 'E'},
//     {'K', '0', 'T', 'N'}};

// byte rowPins[ROWS] = {15, 14, 13, 12, 11}; // connect to the row pinouts of the keypad
// byte colPins[COLS] = {5, 8, 9, 10};
// Keypad_MC17 customKeypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS, I2CADDR);

// Fungsi set Flow Cyble

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
  Serial1.begin(115200, SERIAL_8N1, 14, 15);
  // Wire.begin();
  // customKeypad.begin();
  ads.begin();

  lcd.init();
  lcd.backlight();
  lcd.setCursor(5, 0);
  lcd.print("i-DasWmeter");
  lcd.setCursor(0, 2);
  lcd.print("Stan : ");
  lcd.setCursor(18, 2);
  lcd.print("m3");
  // lcd.setCursor(0, 3);
  // lcd.print("Batt : ");
  // lcd.setCursor(19, 3);
  // lcd.print("%");

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
    gps.encode(Serial1.read());
  }
  if (gps.location.isUpdated())
  {
    StaticJsonDocument<500> doc;
    EepromStream eepromStream(0, 500);

    latitude = gps.location.lat();
    longitude = gps.location.lng();
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
  dataUp["Batt"] = String(persenBatt) + "%";
  dataUp["status"] = status;
  dataUp["flow"] = String(flowSend);
  dataUp["logFlow"] = dataLog;
  serializeJson(dataUp, Serial2);
}

void prosesPengisian()
{

  digitalWrite(pinValve, HIGH);
  Serial.println(CybleCounter_LF);

  if (literCounter == jumlahPesanan)
  {
    digitalWrite(pinValve, LOW);
    flowSend = String(CybleCounter_LF) + "," + String(literCounter);
    dataLog = String(cybleSebelumnya) + "," + String(literSebelumnya);
    status = "Transaction Done";
    logCyble = CybleCounter_LF;
    logLiter = literCounter;

    dataUplink();
    delay(5000);
    dataUplink();
    delay(5000);
    dataUplink();
    delay(5000);
    dataUplink();
    delay(15000);
    resetFunc();
    idle = 1;
  }
}

void dataDownlink()
{
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

      jumlahPesanan = txInfo_item_transaction_volume;

      if (String(txInfo_item_forID) == idDevice)
      {
        Serial.print("ID :");
        Serial.print(txInfo_item_forID);
        Serial.print(" status : ");
        Serial.println(txInfo_item_transaction_status);
        if (txInfo_item_transaction_status == 1)
        {
          cybleSebelumnya = logCyble;
          literSebelumnya = logLiter;
          status = "Transaction Received";
          lcd.clear();
          lcd.setCursor(2, 0);
          lcd.print("PROSES PENGISIAN");
          lcd.setCursor(0, 1);
          lcd.print("Req  : ");
          lcd.setCursor(14, 1);
          lcd.print((float(jumlahPesanan) / 1000), 1);
          lcd.setCursor(17, 1);
          lcd.print(" m3");
          lcd.setCursor(0, 2);
          lcd.print("Stan : ");
          lcd.setCursor(17, 2);
          lcd.print(" m3");
          lcd.setCursor(0, 3);
          lcd.print("Count: ");
          lcd.setCursor(14, 3);
          lcd.print("0.0 ");
          lcd.print("m3");

          dataUplink();
          idle = 0;
          // delay(2000);
        }
      }
    }
  }
}

void bacaBattrey()
{
  int16_t adc0, adc1, adc2, adc3;

  adc0 = ads.readADC_SingleEnded(0);
  adc1 = ads.readADC_SingleEnded(1);
  adc2 = ads.readADC_SingleEnded(2);
  adc3 = ads.readADC_SingleEnded(3);

  battVolt = (6.5078 * (adc0 * multiplier / 1000)) + 0.0321;
  persenBatt = (battVolt / 26) * 100;

  if (persenBatt > 100)
  {
    persenBatt = 100;
  }
  if (persenBatt < 0)
  {
    persenBatt = 0;
  }

  // Serial.print("Batt: ");
  // Serial.println(persenBatt);
}

void loop()
{
  SensorState_LF = digitalRead(LF_State);

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

      // Serial.println("data simpan");
      if (idle == 0)
      {
        literCounter = literCounter + 100;
        if (((float(CybleCounter_LF) / 10), 1) < 1000 && ((float(CybleCounter_LF) / 10), 1) > 99)
        {

          lcd.setCursor(12, 2);
          lcd.print((float(CybleCounter_LF) / 10), 1);
        }

        if (((float(CybleCounter_LF) / 10), 1) < 100)
        {

          lcd.setCursor(13, 2);
          lcd.print((float(CybleCounter_LF) / 10), 1);
        }

        if ((float(literCounter) / 1000) < 10)
        {
          lcd.setCursor(14, 3);
          lcd.print(float(literCounter) / 1000);
        }
        if ((float(literCounter) / 1000) < 100 && (float(literCounter) / 1000) > 9)
        {
          lcd.setCursor(13, 3);
          lcd.print(float(literCounter) / 1000);
        }
      }
    }
    lastSensorState_LF = SensorState_LF;
  }

  if (idle == 1)
  {
    bacaBattrey();
    readGPS();
    dataDownlink();
    Serial.println(CybleCounter_LF);
    literCounter = 0;

    if (((float(CybleCounter_LF) / 10), 1) < 1000 && ((float(CybleCounter_LF) / 10), 1) > 99)
    {

      lcd.setCursor(12, 2);
      lcd.print((float(CybleCounter_LF) / 10), 1);
    }

    if (((float(CybleCounter_LF) / 10), 1) < 100)
    {

      lcd.setCursor(13, 2);
      lcd.print((float(CybleCounter_LF) / 10), 1);
    }

    // if (persenBatt < 10)
    // {
    //   lcd.setCursor(15, 3);
    //   lcd.print("");
    //   lcd.setCursor(16, 3);
    //   lcd.print("");
    //   lcd.setCursor(17, 3);
    //   lcd.print(persenBatt);
    // }
    // if (persenBatt < 100 && persenBatt > 9)
    // {
    //   lcd.setCursor(15, 3);
    //   lcd.print("");
    //   lcd.setCursor(16, 3);
    //   lcd.print(persenBatt);
    // }
    // if (persenBatt > 99)
    // {
    //   lcd.setCursor(15, 3);
    //   lcd.print(persenBatt);
    // }

    if (millis() >= upIdleTime_now + periodUp_idle)
    {
      upIdleTime_now += periodUp_idle;

      flowSend = "0,0";
      dataLog = String(logCyble) + "," + String(logLiter);
      Serial.println("Kondisi idle");
      status = "Ok";
      dataUplink();
    }
  }

  if (idle == 0)
  {
    prosesPengisian();
  }
  else
  {
    digitalWrite(pinValve, LOW);
  }
}
