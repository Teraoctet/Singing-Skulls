#include <ESP8266WiFi.h>

const String FIRMWARE_VERSION = "SS-v3.6";

//#define MULTI_SERVO // COMMENTER POUR LA CHORALE

/////////////////
// ID and NAME //
/////////////////
#ifndef MULTI_SERVO
const byte SKULL_ID = 3; // CHANGER LE NUMERO DU CRANE ICI : 1 à 4
#else
const byte SKULL_ID = 0; // ne pas toucher
#endif
String SKULL_NAMES[5] = { "Jack", "Pat", "Jerry", "Ninon", "Sissi"};
const String SKULL_NAME = SKULL_NAMES[SKULL_ID];

#ifdef MULTI_SERVO                                                      
struct MOTOR {
  uint8_t minVal;
  uint8_t midVal;
  uint8_t maxVal;
};

MOTOR Motors[3] = {
  {125 , 150, 170}, // AAM = 0
  {25 , 49, 75},    // YES = 1
  {0 , 90, 180},    // NO = 2
  {50 , 50, 90},    // JAW = 3
  {55 , 72, 90},    // EYES_Y = 4
  {69 , 78, 96},    // EYES_X = 5
};
#endif

///////////
// Servo //
///////////
#ifndef MULTI_SERVO
#include <Servo.h>
const int SERVO_PIN = 5;
Servo servo;
#else
#include <Adafruit_PWMServoDriver.h>
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
#define SERVOMIN  150 // this is the 'minimum' pulse length count (out of 4096)
#define SERVOMAX  574 // this is the 'maximum' pulse length count (out of 4096)
#endif


///////////
// Serial /
///////////
#define INPUT_SIZE 3
char incBuffer[INPUT_SIZE];
unsigned long pingTimeInMs = 1000;
unsigned long lastPingTime = 0;


void setup()
{
  // disable WIFI on ESP8266
  WiFi.mode( WIFI_OFF );
  WiFi.forceSleepBegin();

  // set up servo
#ifndef MULTI_SERVO
  pinMode(SERVO_PIN, OUTPUT);
  servo.attach(SERVO_PIN);
  servo.write(90); // init servo at mid-range
#else
  pwm.begin();
  pwm.setPWMFreq(60);  // Analog servos run at ~60 Hz updates
  // center motors
  pwm.setPWM(Motors[AAM].index, 0, map(Motors[AAM].midVal, 0, 180, SERVOMIN, SERVOMAX)); // aam (max interne 180, max externe 170)
  pwm.setPWM(1, 0, map(50, 0, 180, SERVOMIN, SERVOMAX)); //yes (min interne 0, min externe 15)
  pwm.setPWM(2, 0, map(90, 0, 180, SERVOMIN, SERVOMAX)); // no
  pwm.setPWM(3, 0, map(45, 0, 180, SERVOMIN, SERVOMAX)); // jaw (min 45, max 90)
  pwm.setPWM(4, 0, map(90, 0, 180, SERVOMIN, SERVOMAX)); // eyes Y
  pwm.setPWM(5, 0, map(93, 0, 180, SERVOMIN, SERVOMAX)); // eyes X

#endif

  Serial.begin(115200);
  delay(2000);
  handshake();
}

void handshake()
{
  Serial.println("name:" + String(SKULL_NAME));
  Serial.println("firmware:" + String(FIRMWARE_VERSION));
}

void loop()
{
  if (millis() - lastPingTime > pingTimeInMs)
  {
    Serial.println("id:" + String(SKULL_ID));
    lastPingTime = millis();
  }

  while (Serial.available()) {
    byte inputSize = Serial.readBytesUntil(char(255), incBuffer, INPUT_SIZE);

    Serial.println("s:" + String(inputSize));
    Serial.println("b:" + String(incBuffer));

    if (incBuffer[0] == 'h') // 104 - x68
    {
      handshake();
      return;
    }

    if (inputSize < 2)
      return;

    int servoValue = int(incBuffer[1]);

    if (incBuffer[0] == 's') // 115 - x73
    {
      if (servoValue < 45 || servoValue > 90)
        Serial.print("value error:" + String(servoValue)); // FIXME: pourquoi ca fait lagger chataigne ?
      else
        servo.write(servoValue);
      return;
    }

#ifdef MULTI_SERVO
    int servoIndex = int(incBuffer[0]);
    if (servoIndex < 0 || servoIndex > Motors.length())
    {
      Serial.print("index error:" + String(servoIndex));
      return;
    }
    Motor m = Motors[servoIndex];

    if (servoValue < m.minVal || servoValue > m.maxVal)
    {
      Serial.print("value error:" + String(servoValue));
      Serial.print("on index:" + String(servoIndex));
    }
    else
      pwm.setPWM(servoIndex, 0, map(servoValue, 0, 180, SERVOMIN, SERVOMAX));
#endif
  }
}
