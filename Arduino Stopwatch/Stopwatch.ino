/**
 * IoT Alarm Clock - Arduino UNO R4 WiFi + Blynk
 * 
 * Hardware: RGB LCD, Buzzer (pin 6), Button (pin 5)
 * 
 * Blynk Virtual Pins:
 *   V0 - Time Input widget (set alarm time)
 *   V1 - Label widget (current time display)
 *   V2 - Label widget (countdown / alarm status)
 *   V3 - Button widget (snooze/reset alarm)
 * 
 * Blynk link: https://blynk.cloud/dashboard/172667/global/devices/1/organization/172667/devices/550550/dashboard
 *
 * To use the blynk app widget board we set up via the above link, we couldn't get it to work because it kept saying to other users that they "lack permissions" so to access it, you can use the following login:
 * Email: Kordian.m06@gmail.com
 * Password: 22q%F96_rr  
 */

#define BLYNK_TEMPLATE_ID "TMPL4EiP2zLOs"
#define BLYNK_TEMPLATE_NAME "Alarm Clock"
#define BLYNK_AUTH_TOKEN "Cr7D6rEa7N_Pg71LKdyoyTW7hyQCXtpX"

#define BLYNK_PRINT Serial

#include <BlynkSimpleWifi.h> 
#include "RTC.h"
#include "rgb_lcd.h"
#include <NTPClient.h>
#include <WiFiS3.h>
#include <WiFiUdp.h>

//  Pin config 
const int buzzerPin = 6;
const int buttonPin = 5;

//  WiFi credentials 
char ssid[] = "Kordian's A36";
char pass[] = "password";

// Alarm state
long alarmUnixTime   = 0;      // absolute unix time the alarm fires
long alarmInterval   = 100000; // default interval (seconds) used by physical button reset
bool alarmSet        = false;  // true once user sets alarm via Blynk
bool alarmFiring     = false;  // true while alarm is going off
int  alarmHour       = -1;     // hour set from Blynk Time Input
int  alarmMinute     = -1;     // minute set from Blynk Time Input

//  LCD 
rgb_lcd lcd;

// NTP
WiFiUDP Udp;
NTPClient timeClient(Udp);

// Blynk timer 
BlynkTimer timer;

// Blynk: V0 — Time Input widget sends alarm hour/minute
BLYNK_WRITE(V0) {
  TimeInputParam t(param);

  if (t.hasStartTime()) {
    alarmHour   = t.getStartHour();
    alarmMinute = t.getStartMinute();

    // Calculate the next unix time
    RTCTime currentTime;
    RTC.getTime(currentTime);
    long nowUnix = currentTime.getUnixTime();

    // Build today's alarm unix time
    // Start from midnight of today then add hours + minutes
    long midnightToday = nowUnix - (currentTime.getHour() * 3600)
                                 - (currentTime.getMinutes() * 60)
                                 - currentTime.getSeconds();
    alarmUnixTime = midnightToday + (alarmHour * 3600) + (alarmMinute * 60);

    // If that time has already passed today, schedule for tomorrow
    if (alarmUnixTime <= nowUnix) {
      alarmUnixTime += 86400;
    }

    alarmSet     = true;
    alarmFiring  = false;
    noTone(buzzerPin);
    lcd.setRGB(0, 255, 0);

    Serial.print("Alarm set for ");
    Serial.print(alarmHour);
    Serial.print(":");
    if (alarmMinute < 10) Serial.print("0");
    Serial.println(alarmMinute);

    Blynk.virtualWrite(V2, String("Alarm set: ") + alarmHour + ":" + (alarmMinute < 10 ? "0" : "") + alarmMinute);
  }
}

// Blynk: V3 — button from app
BLYNK_WRITE(V3) {
  if (param.asInt() == 1) {
    snoozeAlarm();
  }
}


// Reset logic: button + Blynk
void snoozeAlarm() {
  noTone(buzzerPin);
  lcd.setRGB(0, 255, 0);
  lcd.clear();
  alarmFiring = false;

  RTCTime currentTime;
  RTC.getTime(currentTime);

  if (alarmSet && alarmHour >= 0) {
    // Re-schedule for next day at same time
    long midnightToday = currentTime.getUnixTime()
                        - (currentTime.getHour() * 3600)
                        - (currentTime.getMinutes() * 60)
                        - currentTime.getSeconds();
    alarmUnixTime = midnightToday + 86400 + (alarmHour * 3600) + (alarmMinute * 60);
    Blynk.virtualWrite(V2, "Alarm reset for tomorrow");
  } else {
    
    alarmUnixTime = currentTime.getUnixTime() + alarmInterval;
    Blynk.virtualWrite(V2, "Alarm reset (interval)");
  }
}

// Timer callback: update Blynk app + LCD every second
void updateDisplay() {
  RTCTime currentTime;
  RTC.getTime(currentTime);

  // Format time string
  String timeStr = "";
  timeStr += currentTime.getHour();
  timeStr += ":";
  if (currentTime.getMinutes() < 10) timeStr += "0";
  timeStr += currentTime.getMinutes();
  timeStr += ":";
  if (currentTime.getSeconds() < 10) timeStr += "0";
  timeStr += currentTime.getSeconds();

  // Send to BlynK
  Blynk.virtualWrite(V1, timeStr);

  long secondsLeft = alarmUnixTime - currentTime.getUnixTime();

  if (alarmFiring) {
    Blynk.virtualWrite(V2, "ALARM!");
    
  } else if (alarmSet || secondsLeft > 0) {
    long mins = secondsLeft / 60;
    long secs = secondsLeft % 60;
    String countdown = String(mins) + "m " + String(secs) + "s left";
    Blynk.virtualWrite(V2, countdown);

    // Update LCD
    lcd.setCursor(0, 0);
    lcd.print(timeStr + "  ");   // extra spaces clear leftover chars
    lcd.setCursor(0, 1);
    lcd.print(countdown + "  ");
  } else {
    Blynk.virtualWrite(V2, "No alarm set");
    lcd.setCursor(0, 0);
    lcd.print(timeStr + "  ");
    lcd.setCursor(0, 1);
    lcd.print("No alarm set    ");
  }
}

void connectToWiFi() {
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("WiFi module failed!");
    while (true);
  }

  int status = WL_IDLE_STATUS;
  while (status != WL_CONNECTED) {
    Serial.print("Connecting to: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(5000);
  }
  Serial.println("WiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);

  lcd.begin(16, 2);
  lcd.setRGB(0, 255, 0);

  Serial.begin(9600);
  while (!Serial);

  connectToWiFi();

  // Sync RTC via NTP
  RTC.begin();
  timeClient.begin();
  timeClient.update();

  int tzOffsetHours = 1; // Change to your UTC offset
  long unixTime = timeClient.getEpochTime() + (tzOffsetHours * 3600);
  RTCTime timeToSet = RTCTime(unixTime);
  RTC.setTime(timeToSet);

  RTCTime currentTime;
  RTC.getTime(currentTime);
  Serial.println("RTC set to: " + String(currentTime));

  // Default interval alarm (until user sets one via Blynk)
  alarmUnixTime = currentTime.getUnixTime() + alarmInterval;

  // Connect to Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Update display every second
  timer.setInterval(1000L, updateDisplay);

  lcd.clear();
  lcd.print("  Clock Ready");
}

void loop() {
  Blynk.run();
  timer.run();

  RTCTime currentTime;
  RTC.getTime(currentTime);

  // Physical button
  if (digitalRead(buttonPin) == LOW) {
    snoozeAlarm();
    delay(300); // debounce
  }

  //Check if alarm should fire
  if (!alarmFiring && currentTime.getUnixTime() >= alarmUnixTime) {
    alarmFiring = true;
    tone(buzzerPin, 1000);
    lcd.setRGB(255, 0, 0);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ALARM! WAKE UP!");
    Blynk.virtualWrite(V2, "ALARM FIRING!");
    Blynk.logEvent("alarm_trigger", "Your alarm is going off!"); // push notification
  }

  // Keep LCD showing alarm message while firing
  if (alarmFiring) {
    lcd.setCursor(0, 0);
    lcd.print("ALARM! WAKE UP!");
    lcd.setCursor(0, 1);
    lcd.print("Press button!   ");
  }
}
