#include <Wire.h>
#include "RTClib.h"
#include <SPI.h>
#include <SD.h>
#include <DHT.h>
#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>

// === Пины ===
const int chipSelect = 4;
#define DHTPIN 2
#define DHTTYPE DHT22
#define CO2_RX_PIN A1
#define CO2_TX_PIN A0

// === Тайминги ===
const unsigned long DHT_INTERVAL     = 2000;
const unsigned long CO2_INTERVAL     = 5000;
const unsigned long LOG_INTERVAL     = 5000;
const unsigned long DISPLAY_INTERVAL = 3000;   // ← время показа одного экрана

// === Объекты ===
DHT dht(DHTPIN, DHTTYPE);
RTC_DS3231 rtc;
SoftwareSerial co2Serial(CO2_TX_PIN, CO2_RX_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Команда MH-Z19B
const byte CO2_CMD[9] = {0xFF,0x01,0x86,0,0,0,0,0,0x79};

// Показатели
float lastTemp = NAN, lastHumi = NAN;
int   lastCO2  = -1;

// Служебные переменные времени
unsigned long prevDHT = 0, prevCO2 = 0, prevLog = 0, prevDisp = 0;

// Буферы
char dateBuf[11], timeBuf[9];

// Текущий «экран» 0 — дата/время, 1 — датчики
byte screen = 0;

// Короткое имя файла
char filename[13];  // 8 символов + .csv + \0

void setup() {
  Serial.begin(9600);
  Wire.begin();

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Initializing...");

  if (!rtc.begin()) {
    lcd.clear(); lcd.print("RTC Error!");
    while (1);
  }

  if (rtc.lostPower()) {
//rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}

  dht.begin();
  co2Serial.begin(9600);

  if (!SD.begin(chipSelect)) {
    lcd.clear(); lcd.print("SD Error!");
    while (1);
  }

  // Создание короткого имени файла 8+3
  DateTime now = rtc.now();
  sprintf(filename, "%02d%02d%02d%02d.csv", now.day(), now.hour(), now.minute(), now.second());
  // Пример: 18092315.csv

  File dataFile = SD.open(filename, FILE_WRITE);
  if (dataFile) {
    dataFile.println("Date,Time,Temp(C),Hum(%),CO2(ppm)");
    dataFile.close();
  } else {
    lcd.clear(); lcd.print("File Error!");
    while (1);
  }

  delay(1500);
  lcd.clear();
}

void loop() {
  unsigned long nowMs = millis();

  /* ---------- Чтения датчиков ---------- */
  if (nowMs - prevDHT >= DHT_INTERVAL) {
    prevDHT = nowMs;
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (!isnan(h) && !isnan(t)) { lastHumi = h; lastTemp = t; }
  }

  if (nowMs - prevCO2 >= CO2_INTERVAL) {
    prevCO2 = nowMs;
    int co2 = readCO2();
    if (co2 != -1) lastCO2 = co2;
  }

  /* ---------- Запись на SD ---------- */
  if (nowMs - prevLog >= LOG_INTERVAL) {
    prevLog = nowMs;

    DateTime ts = rtc.now();
    sprintf(dateBuf, "%04d-%02d-%02d", ts.year(), ts.month(), ts.day());
    sprintf(timeBuf, "%02d:%02d:%02d", ts.hour(), ts.minute(), ts.second());

    File f = SD.open(filename, FILE_WRITE);
    if (f) {
      f.print(dateBuf); f.print(','); f.print(timeBuf); f.print(',');
      f.print(lastTemp,1); f.print(','); f.print(lastHumi,1); f.print(',');
      f.println(lastCO2 != -1 ? String(lastCO2) : "Err");
      f.close();
      flashRec(); // мигаем индикатором записи
    }
  }

  /* ---------- Переключение экранов ---------- */
  if (nowMs - prevDisp >= DISPLAY_INTERVAL) {
    prevDisp = nowMs;
    screen ^= 1;                         // 0 → 1 → 0 …
    updateDisplay();
  }
}

/* ===== Функции ===== */

void updateDisplay() {
  lcd.clear();
  if (screen == 0) {                     // Экран 1: дата + время
    DateTime t = rtc.now();
    sprintf(dateBuf, "%04d-%02d-%02d", t.year(), t.month(), t.day());
    sprintf(timeBuf, "%02d:%02d:%02d", t.hour(), t.minute(), t.second());
    lcd.setCursor(0,0); lcd.print(dateBuf);
    lcd.setCursor(0,1); lcd.print(timeBuf);
  } else {                               // Экран 2: датчики
    lcd.setCursor(0,0);
    lcd.print("T:"); lcd.print(lastTemp,1); lcd.print("C ");
    lcd.print("H:"); lcd.print(lastHumi,0); lcd.print("%");
    lcd.setCursor(0,1);
    lcd.print("CO2:");
    if (lastCO2 != -1) lcd.print(lastCO2);
    else               lcd.print("Err");
    lcd.print("ppm");
  }
}

void flashRec() {                        
  lcd.setCursor(15,1); lcd.write(byte(255));
  delay(300);
  lcd.setCursor(15,1); lcd.print(' ');
}

int readCO2() {
  co2Serial.write(CO2_CMD, 9);
  byte resp[9];
  if (co2Serial.readBytes(resp, 9) == 9) return (resp[2]<<8)|resp[3];
  return -1;
}
