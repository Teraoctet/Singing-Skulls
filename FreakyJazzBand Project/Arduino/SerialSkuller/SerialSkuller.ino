const String FIRMWARE_VERSION = "SS-v3.0";

//#define MULTI_SERVO // COMMENT FOR CHOIR SKULLS


/////////////////
// ID and NAME //
/////////////////
#ifndef MULTI_SERVO
const int SKULL_ID = 1; // SET SKULL ID HERE: 1 to 7
#else
const int SKULL_ID = 0; // do not change
#endif
String SKULL_NAMES[6] = { "Jack", "Pat", "Ninon", "Sissi", "Jerry", "Hubert"};
const String SKULL_NAME = SKULL_NAMES[SKULL_ID];

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
#define INPUT_SIZE 50
char incBuffer[INPUT_SIZE];
int pingTimeInMs = 1000;
unsigned long lastPingTime = 0;


void setup()
{
  // set up servo
#ifndef MULTI_SERVO
  pinMode(SERVO_PIN, OUTPUT);
  servo.attach(SERVO_PIN);
  servo.write(90); // init servo at mid-range
#else
  pwm.begin();
  pwm.setPWMFreq(60);  // Analog servos run at ~60 Hz updates
#endif

  Serial.begin(115200);
  delay(2000);
  handshake();
}

void handshake()
{
  Serial.println("name:" + String(SKULL_NAME));
  Serial.println("firmware:" + String(FIRMWARE_VERSION));
  // local IP
  // target IP
}

void loop()
{
  if ((unsigned long)(millis() - lastPingTime) > pingTimeInMs)
  {
    //Serial.println("ping");
    Serial.println("id:" + String(SKULL_ID));
    lastPingTime = millis();
  }

  while (Serial.available()) {
    byte inputSize = Serial.readBytesUntil('\n', incBuffer, INPUT_SIZE);
    if (incBuffer[inputSize] == '\r')
    {
      //incBuffer[inputSize] = "";
      inputSize--;
    }

    // Answer to handshake
    if (!strcmp(incBuffer, "handshake"))
      handshake();

    // Otherwise look for separator
    char* separator = strchr(&incBuffer[0], ':');
    if (separator)
    {
      int servoIndex = atoi(incBuffer);

      *separator = 0;
      ++separator;

      int servoValue = constrain(atoi(separator), 0, 180);
      Serial.println("servoValue:" + String(servoValue));
#ifndef MULTI_SERVO
      servo.write(servoValue);
#else
      pwm.setPWM(servoIndex, 0, map(servoValue, 0, 180, SERVOMIN, SERVOMAX));
#endif
    }
  }
}
