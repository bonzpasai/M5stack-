#include <Arduino.h>
#include <M5Stack.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>
#include <math.h>
#include <SD.h>

#define LED_PIN 2
#define LED_PIN2 5

const char* ssid = "Myiphone12";
const char* password = "abcd12222";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 9 * 3600, 60000);

String currentCity = "Anan";
float lat = 33.92;
float lon = 134.65;
int weatherCode;
bool isGraphShowing = false;
unsigned long graphStartTime = 0;
const unsigned long graphDuration = 10000;

float currentTemperature = 0.0;
String todayForecast = "";
float todayPrecipitationGraph[24];
float hourlyTemp[24];
float hourlyRain[24];
char dailyForecast[31];

void drawAnalogClock(int x, int y, int radius) {
  time_t rawTime = timeClient.getEpochTime();
  struct tm* t = localtime(&rawTime);
  int hour = t->tm_hour % 12;
  int min = t->tm_min;
  int sec = t->tm_sec;
  float ha = (hour + min / 60.0) * 30;
  float ma = min * 6;
  float sa = sec * 6;

  M5.Lcd.drawCircle(x, y, radius, WHITE);
  M5.Lcd.fillCircle(x, y, radius - 1, BLACK);
  for (int i = 1; i <= 12; i++) {
    float a = (i * 30 - 90) * DEG_TO_RAD;
    int tx = x + cos(a) * (radius - 12);
    int ty = y + sin(a) * (radius - 12);
    M5.Lcd.setCursor(tx - 4, ty - 4);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setTextSize(1);
    M5.Lcd.print(i);
  }
  M5.Lcd.drawLine(x, y, x + cos((ha - 90) * DEG_TO_RAD) * radius * 0.5,
                  y + sin((ha - 90) * DEG_TO_RAD) * radius * 0.5, GREEN);
  M5.Lcd.drawLine(x, y, x + cos((ma - 90) * DEG_TO_RAD) * radius * 0.8,
                  y + sin((ma - 90) * DEG_TO_RAD) * radius * 0.8, BLUE);
  M5.Lcd.drawLine(x, y, x + cos((sa - 90) * DEG_TO_RAD) * radius * 0.9,
                  y + sin((sa - 90) * DEG_TO_RAD) * radius * 0.9, RED);
}

void drawCalendar(int startX, int startY) {
  time_t rawTime = timeClient.getEpochTime();
  struct tm* t = localtime(&rawTime);
  int year = t->tm_year + 1900;
  int month = t->tm_mon + 1;
  int today = t->tm_mday;

  struct tm firstDay = {0};
  firstDay.tm_year = t->tm_year;
  firstDay.tm_mon = t->tm_mon;
  firstDay.tm_mday = 1;
  mktime(&firstDay);
  int startDay = firstDay.tm_wday;

  const char* days[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
  int dim[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) dim[1] = 29;

  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(startX, startY - 12);
  M5.Lcd.printf("%04d/%02d/%02d  %.1f C  %s", year, month, today, currentTemperature, todayForecast.c_str());


  for (int i = 0; i < 7; i++) {
    M5.Lcd.setCursor(startX + i * 24, startY);
    M5.Lcd.setTextColor(i == 0 || i == 6 ? RED : WHITE, BLACK);
    M5.Lcd.print(days[i]);
  }

  for (int d = 1; d <= dim[t->tm_mon]; d++) {
    int x = (startDay + d - 1) % 7;
    int y = (startDay + d - 1) / 7 + 1;
    int posX = startX + x * 24;
    int posY = startY + y * 14;

    if (d == today) M5.Lcd.drawCircle(posX + 4, posY + 5, 8, YELLOW);
    M5.Lcd.setCursor(posX, posY);
    M5.Lcd.setTextColor(x == 0 || x == 6 ? RED : WHITE, BLACK);

    if (d >= today && d < today + 7) {
      M5.Lcd.printf("%2d%c", d, dailyForecast[d - today]);
    } else {
      M5.Lcd.printf("%2d", d);
    }
  }
}

void getWeather() {
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(lat, 4) + "&longitude=" + String(lon, 4)
              + "&current_weather=true&daily=weathercode&hourly=temperature_2m,precipitation&timezone=auto";
  HTTPClient http;
  http.begin(url);
  int code = http.GET();

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.printf("%s\n", currentCity.c_str());

  for (int i = 0; i < 31; i++) dailyForecast[i] = ' ';

  if (code == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(8192);
    deserializeJson(doc, payload);
    currentTemperature = doc["current_weather"]["temperature"];
    weatherCode = doc["daily"]["weathercode"][0];

    JsonArray codes = doc["daily"]["weathercode"];
    JsonArray temps = doc["hourly"]["temperature_2m"];
    JsonArray rains = doc["hourly"]["precipitation"];

    for (int i = 0; i < 24; i++) {
      hourlyTemp[i] = temps[i];
      hourlyRain[i] = rains[i];
      todayPrecipitationGraph[i] = rains[i];
    }

    for (int i = 0; i < codes.size() && i < 31; i++) {
      int w = codes[i];
      dailyForecast[i] = (w < 3) ? 'S' : (w < 6) ? 'C' : 'R';
    }

    if (codes.size() > 0) {
      int w = codes[0];
      todayForecast = (w < 3) ? "Sunny" : (w < 6) ? "Cloudy" : "Rain";
    }
  } else {
    M5.Lcd.println("Weather error");
  }
  http.end();
  drawCalendar(10, 160);
}

void drawPrecipitationGraph(float* data) {
  int barWidth = 10, graphHeight = 160, baseY = 190, xStart = 5;
  M5.Lcd.fillScreen(BLACK);
  float maxVal = *std::max_element(data, data + 24);
  if (maxVal == 0) maxVal = 1;
  for (int i = 0; i < 24; i++) {
    int barH = (int)(data[i] / maxVal * graphHeight);
    int x = xStart + i * (barWidth + 1);
    uint16_t color = (i == timeClient.getHours()) ? YELLOW : BLUE;
    M5.Lcd.fillRect(x, baseY - barH, barWidth, barH, color);
  }
  M5.Lcd.setCursor(10, 10);
  M5.Lcd.setTextColor(YELLOW, BLACK);
  M5.Lcd.println("Precipitation (mm)");
}

void drawTemperatureLineGraph(float* data) {
  int graphHeight = 160, baseY = 190, xStart = 5, xStep = 13;
  float maxVal = -100, minVal = 100;
  for (int i = 0; i < 24; i++) {
    maxVal = max(data[i], maxVal);
    minVal = min(data[i], minVal);
  }
  if (maxVal == minVal) maxVal = 1;
  for (int i = 1; i < 24; i++) {
    int x1 = xStart + (i - 1) * xStep;
    int y1 = baseY - (int)((data[i - 1] - minVal) / (maxVal - minVal) * graphHeight);
    int x2 = xStart + i * xStep;
    int y2 = baseY - (int)((data[i] - minVal) / (maxVal - minVal) * graphHeight);
    M5.Lcd.drawLine(x1, y1, x2, y2, RED);
  }
  M5.Lcd.setCursor(10, 30);
  M5.Lcd.setTextColor(RED, BLACK);
  M5.Lcd.println("Temperature (C)");
}

void setup() {
  M5.begin(); SD.begin(); pinMode(2, OUTPUT); pinMode(5, OUTPUT); pinMode(26, OUTPUT);
  Serial.begin(9600);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2); M5.Lcd.println("Connecting WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); M5.Lcd.print("."); }
  M5.Lcd.println("\nConnected!");
  timeClient.begin(); timeClient.update(); getWeather();
}

void loop() {
  timeClient.update(); M5.update();
  if (!isGraphShowing) {
    drawAnalogClock(255, 68, 60);
    if (currentCity == "Anan" && SD.exists("/img/ananan.png")) M5.Lcd.drawPngFile(SD, "/img/ananan.png", 240, 180);
    else if (currentCity == "Tokyo" && SD.exists("/img/tokyo.png")) M5.Lcd.drawPngFile(SD, "/img/tokyo.png", 240, 150);
    else if (currentCity == "Laos" && SD.exists("/img/lao.png")) M5.Lcd.drawPngFile(SD, "/img/lao.png", 240, 180);
  }

  if (M5.BtnA.wasPressed()) {
    if (currentCity == "Anan") { currentCity = "Tokyo"; lat = 35.68; lon = 139.76; timeClient.setTimeOffset(9 * 3600); }
    else if (currentCity == "Tokyo") { currentCity = "Laos"; lat = 17.97; lon = 102.60; timeClient.setTimeOffset(7 * 3600); }
    else { currentCity = "Anan"; lat = 33.92; lon = 134.65; timeClient.setTimeOffset(9 * 3600); }
    getWeather();
  }

  if (M5.BtnB.wasPressed() && !isGraphShowing) {
    isGraphShowing = true; graphStartTime = millis();
    drawPrecipitationGraph(todayPrecipitationGraph);
    drawTemperatureLineGraph(hourlyTemp);
  }

  if (M5.BtnC.wasPressed() && isGraphShowing) {
    isGraphShowing = false; getWeather();
  }

  if (weatherCode < 3) {
    M5.Lcd.drawPngFile(SD, "/img/sun.png", 100, 30);
    digitalWrite(2, HIGH); digitalWrite(5, LOW);
  } else if (weatherCode < 6) {
    M5.Lcd.drawPngFile(SD, "/img/cloud.png", 100, 30);
    digitalWrite(2, HIGH); digitalWrite(5, LOW);
    delay(50);
    digitalWrite(2, LOW); digitalWrite(5, HIGH); 
  } else {
    M5.Lcd.drawPngFile(SD, "/img/rain.png", 100, 30);
    digitalWrite(2, LOW); digitalWrite(5, HIGH);
  }
  delay(100);
}