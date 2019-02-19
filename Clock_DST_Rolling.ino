/*
 * ESP8266 WiFi to Analog Voltmeter Clock
 *
*/

#include <ESP8266WiFi.h>
#include <time.h>                       // time() ctime()

////////////////////////////////////////////////////////
//
// WiFi setup
//
#include <WiFiManager.h>

#define SSID            "ThisNetworkIsGUEST"
#define SSIDPWD         "your-ssid_password"


////////////////////////////////////////////////////////
//
//ESP 8266 PIN OUTS
//
#define D0 16
#define D1 5 // I2C Bus SCL (clock)
#define D2 4 // I2C Bus SDA (data)
#define D3 0
#define D4 2 // Same as "LED_BUILTIN", but inverted logic
#define D5 14 // SPI Bus SCK (clock)
#define D6 12 // SPI Bus MISO
#define D7 13 // SPI Bus MOSI
#define D8 15 // SPI Bus SS (CS)
#define D9 3 // RX0 (Serial console)
#define D10 1 // TX0 (Serial console)
////////////////////////////////////////////////////////

#define HOURPIN D0
#define HOURLED D4
int HOURPWM = 0;
#define hour_max_pwm 930
#define MINPIN D1
#define MINLED D3
int MINPWM = 0;
#define min_max_pwm 970
#define SECPIN D2
#define SECLED D5
int SECPWM = 0;
#define sec_max_pwm 955

const int HOUR_LOOKUP[] = {11,0,1,2,3,4,5,6,7,8,9,10,11,0,1,2,3,4,5,6,7,8,9,10,11}; //Convert 0-23 hours to 1-12
timeval tv;
struct timezone tz;
timespec tp;
time_t tnow;
struct tm  ts;

void setup() {
  pinMode(HOURLED, OUTPUT);
  pinMode(SECLED, OUTPUT);
  pinMode(MINLED, OUTPUT);
  digitalWrite(HOURLED, HIGH);
  digitalWrite(MINLED, LOW);
  digitalWrite(SECLED, LOW);

  Serial.begin(115200);

  // set time from RTC
  // Normally you would read the RTC to eventually get a current UTC time_t
  // this is faked for now.
  time_t rtc_time_t = 1541267183; // fake RTC time for now

  timezone tz = { 0, 0};
  timeval tv = { rtc_time_t, 0};

  // DO NOT attempt to use the timezone offsets
  // The timezone offset code is really broken.
  // if used, then localtime() and gmtime() won't work correctly.
  // always set the timezone offsets to zero and use a proper TZ string
  // to get timezone and DST support.

  // set the time of day and explicitly set the timezone offsets to zero
  // as there appears to be a default offset compiled in that needs to
  // be set to zero to disable it.
  settimeofday(&tv, &tz);

  // set up TZ string to use a POSIX/gnu TZ string for local timezone
  // TZ string information:
  // https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
  setenv("TZ", "PST+8PDT,M3.2.0/2,M11.1.0/2", 1);

  tzset(); // save the TZ variable

  // turn on WIFI using SSID/SSIDPWD
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, SSIDPWD);

  analogWrite(HOURPIN, hour_max_pwm*1.1); // Move needle to show stage 1, Connecting to WiFi
  analogWrite(MINPIN, 0);
  analogWrite(SECPIN, 0);

  while ( WiFi.status() != WL_CONNECTED ) { //Flash lights when connecting to WiFi
    delay ( 500 );
    Serial.print ( "." );
    if (digitalRead(HOURLED)) {
      digitalWrite(HOURLED, LOW);
      digitalWrite(MINLED, HIGH);
      digitalWrite(SECLED, LOW);
    }
    else if (digitalRead(MINLED)) {
      digitalWrite(HOURLED, LOW);
      digitalWrite(MINLED, LOW);
      digitalWrite(SECLED, HIGH);
    }
    else {
      digitalWrite(HOURLED, HIGH);
      digitalWrite(MINLED, LOW);
      digitalWrite(SECLED, LOW);
    }
  }

  configTime(0, 0, "pool.ntp.org");
  analogWrite(MINPIN, min_max_pwm*1.1); // Move needle to show stage 2, Connecting to NTP server
  while ( ts.tm_year < 119 ) { //Spin until year is 2019 or later
    Serial.print ( "!" );
    if (digitalRead(HOURLED)) {
      digitalWrite(HOURLED, LOW);
      digitalWrite(MINLED, LOW);
      digitalWrite(SECLED, HIGH);
    }
    else if (digitalRead(MINLED)) {
      digitalWrite(HOURLED, HIGH);
      digitalWrite(MINLED, LOW);
      digitalWrite(SECLED, LOW);
    }
    else {
      digitalWrite(HOURLED, LOW);
      digitalWrite(MINLED, HIGH);
      digitalWrite(SECLED, LOW);
    }
    delay ( 500 );
    gettimeofday(&tv, &tz);
    tnow = time(nullptr);
    ts = *localtime(&tnow);
  }
  SECPWM = (ts.tm_sec * sec_max_pwm) / 60; //Calculate seconds PWM
  analogWrite(SECPIN, SECPWM);
  MINPWM = (ts.tm_min * min_max_pwm) / 60; //Calculate minute PWM
  analogWrite(MINPIN, MINPWM);
  HOURPWM = (HOUR_LOOKUP[ts.tm_hour] * hour_max_pwm) / 11; //Calculate hour PWM
  analogWrite(HOURPIN, HOURPWM);
}

void loop()
{
  gettimeofday(&tv, &tz);
  tnow = time(nullptr);
  // localtime / gmtime every second change
  static time_t lastv = 0;
  ts = *localtime(&tnow);

  if (lastv != tv.tv_sec) //only upate on a new second
  {
    lastv = tv.tv_sec;
    printf(" local asctime: %s", asctime(localtime(&tnow))); // print formated local time
    Serial.println();
    SECPWM = (ts.tm_sec * sec_max_pwm) / 60; //Calculate seconds PWM
    analogWrite(SECPIN, SECPWM);
    if (SECPWM == 0) { //Flash light and update minutes and hours on second zero out 
      digitalWrite(SECLED, LOW);
      delay(100);
      analogWrite(SECPIN, 1000);
      delay(40);
      analogWrite(SECPIN, SECPWM);
      MINPWM = (ts.tm_min * min_max_pwm) / 60; //Calculate minute PWM
      analogWrite(MINPIN, MINPWM);
      if (MINPWM == 0){ //Flash light and update hour on minute roll over
        digitalWrite(MINLED, LOW);
        delay(100);
        analogWrite(MINPIN, 1000);
        delay(40);
        analogWrite(MINPIN, MINPWM);
        HOURPWM = (HOUR_LOOKUP[ts.tm_hour] * hour_max_pwm) / 11; //Calculate hour PWM
        analogWrite(HOURPIN, HOURPWM);
        if (HOURPWM == 0) {
          digitalWrite(HOURLED, LOW);
          delay(100);
          analogWrite(HOURPIN, 1000);
          delay(40);
          analogWrite(HOURPIN, HOURPWM);
        }
      }
    }
    delay(430);
    digitalWrite(HOURLED, HIGH);
    digitalWrite(MINLED, HIGH);
    digitalWrite(SECLED, HIGH);
  }
  delay(100);
}
