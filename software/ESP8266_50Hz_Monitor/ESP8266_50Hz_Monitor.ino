#define LED_RED        0
#define LED_BLUE       2
#define PULSE_IN       5
#define PulsesToCount  10
#define MaxTimerCounts 0x7FFFFF

#define LCD_TX 12
#define LCD_RX 13

#define LCDMode0  21                               //Turn the display off
#define LCDMode1  22                               //Turn the display on, with cursor off and no blink
#define LCDMode2  23                               //Turn the display on, with cursor off and character blink
#define LCDMode3  24                               //Turn the display on, with cursor on and no blink (Default)
#define LCDMode4  25                               //Turn the display on, with cursor on and character blink
#define LCDClr  0x0C
#define LCDBackLightOn  0x11
#define LCDBackLightOff  0x12
#define LCDLine1  0x80
#define LCDLine2  0x94
#define LCDLine3  0xA8
#define LCDLine4  0xBC

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <SoftwareSerial.h>
#include "credentials.h"

ESP8266WiFiMulti WiFiMulti;
SoftwareSerial LCD;

const char* ssid = mySSID;
const char* password = myPASSWORD;
const char* sonde = mySonde;
const long interval = 5000; // 5 Sec. 

//Your IP address or domain name with URL path
const char* URL = myURL;

volatile uint32_t TimerValue = 0;
volatile int PulseCounter = 0;
volatile bool MeasureFinished = false;
volatile bool EnablePulseCounter = false;
uint32_t TotalTimerCounts = 0;
float Period = 0;
float Freq = 0;
float LogFreq = 100;
uint32_t previousMillis = 0;

// Interrupt routine
ICACHE_RAM_ATTR void pulseInISR() {
  if (EnablePulseCounter) {
    if (PulseCounter > PulsesToCount) {
      PulseCounter--;
    } else {
      if (PulseCounter == PulsesToCount) {
        timer1_write(MaxTimerCounts);
        PulseCounter--;
        digitalWrite(LED_BLUE, LOW);
      } else {
        if (PulseCounter == 0) {
          TimerValue = timer1_read();
          MeasureFinished = true;
          digitalWrite(LED_BLUE, HIGH);
          EnablePulseCounter = false;
        } else {
          PulseCounter--;
          digitalWrite(LED_BLUE, !digitalRead(LED_BLUE));
        }
      }
    }
  }
}

void connectWifi() {
  Serial.print("Wifi connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  //IP address assigned to your ESP
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n*Start*\n");
  LCD.begin(19200, SWSERIAL_8N1, LCD_RX, LCD_TX, false);
  if (!LCD) { // If the object did not initialize, then its configuration is invalid
    Serial.println("Invalid LCD serial configuration, check config"); 
    while (1) { // Don't continue with invalid configuration
      delay (1000);
    }
  } 
  Serial.println("LCD serial configuration is O.K");
  LCD.write(LCDMode1);
  delay(200);
  LCD.write(LCDClr);
  delay(200);
  
  // I/O
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(PULSE_IN, INPUT);
  digitalWrite(LED_RED, HIGH); 
  digitalWrite(LED_BLUE, HIGH);

  //TIMER1
  timer1_disable();
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
  timer1_write(MaxTimerCounts);
   
  //GPIO 5
  attachInterrupt(digitalPinToInterrupt(PULSE_IN), pulseInISR, RISING);

  // WIFI
  connectWifi();

  // Turn LCD Backlight on
  LCD.write(LCDBackLightOn);

  LCD.write(LCDLine1);
  LCD.print("  Netzfrequenz");
  delay(100);
  
  // save current time
  previousMillis = millis(); 
}

void loop() {
  uint32_t currentMillis = millis();
  String msg = "";
  char buff[10];
  
  PulseCounter = PulsesToCount + 1;
  MeasureFinished = false;
  EnablePulseCounter = true;
  delay(1000);
  if (MeasureFinished == false) {
    Serial.println("No data");
  } else {
    TotalTimerCounts = MaxTimerCounts - TimerValue;
    Period = 0.0000002 * TotalTimerCounts; // 5 us per Timertick
    Freq = PulsesToCount / (Period * 2);
    LCD.write(LCDLine2);
    LCD.print("   ");
    LCD.print(Freq, 4);
    LCD.print(" Hz");
    if (LogFreq > Freq) {
      LogFreq = Freq;
    }
  }

  if(currentMillis - previousMillis >= interval) {
    // Check WiFi connection status
    if ((WiFiMulti.run() == WL_CONNECTED)) {
      msg.concat(URL);
      msg.concat("?sonde=");
      msg.concat(sonde);
      msg.concat("&freq=");
      dtostrf(LogFreq, 4, 4, buff);
      LogFreq = 100;
      msg.concat(buff);
      // Serial.print("HTTP URL = ");
      Serial.println(msg);
      
      WiFiClient client;
      HTTPClient http;
      // Serial.print("[HTTP] begin...\n");
      if (http.begin(client, msg)) {  // HTTP
        // Serial.print("[HTTP] GET...\n");
        // start connection and send HTTP header
        int httpCode = http.GET();
  
        // httpCode will be negative on error
        if (httpCode > 0) {
          // HTTP header has been send and Server response header has been handled
          // Serial.printf("[HTTP] GET... code: %d\n", httpCode);
  
          // file found at server
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            String payload = http.getString();
            // Serial.println(payload);
          }
        } else {
          Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();
      } else {
        Serial.printf("[HTTP} Unable to connect\n");
      }
      
      // save the last HTTP GET Request
      previousMillis = currentMillis;
    }
    else {
      connectWifi();
    }
  }
}
