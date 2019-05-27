#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiUDP.h>
#include <OSCMessage.h>
#include <OSCBundle.h>
#include "WifiUDP.h"
#include <EEPROM.h>

//#define SOLIST // COMMENT FOR NON SOLISTS

/////////////////
// ID and NAME //
/////////////////
#ifndef SOLIST
const int SKULL_ID = 1; // SET SKULL ID HERE: 1 to 7
#else
const int SKULL_ID = 0; // do not change
#endif

String SKULL_NAMES[6] = { "Jack", "Pat", "Ninon", "Jerry", "Sissi", "Hubert"};
const String SKULL_NAME = SKULL_NAMES[SKULL_ID];

//////////
// WiFi //
//////////
WiFiEventHandler gotIpEventHandler, disconnectedEventHandler;
const char* ssid = "LeNet";
const char* password = "connectemoi";
const unsigned int outPort = 12345;
const unsigned int oscPort = 54321;
const unsigned int dataPort = 25500;
const unsigned int tcpPort = 55555;
WiFiUDP UdpOSC;
WiFiUDP UdpData;
char packetBuffer[UDP_TX_PACKET_MAX_SIZE + 1];
IPAddress broadcastIP1(192, 168, 43, 255); // android hotspot
IPAddress broadcastIP2(192, 168, 0, 255);
IPAddress broadcastIP3(192, 168, 1, 255);
IPAddress broadcastIP4(192, 168, 137, 255); // windows 10 hotspot
IPAddress broadcastIP5(172, 20, 10, 15); // iphone hotspot
IPAddress outIp(192, 168, 43, 255); // will be set by the EEPROM or the handshake
bool connectedToWiFi = false;
bool gotHandshake = false;

int pingTimeInMs = 1000;
unsigned long lastPingTime = 0;
int treatedPackets = 0;

///////////
// Servo //
///////////
const int SERVO_INIT_VALUE = 90; // init servo at mid position

#ifndef SOLIST
#include <Servo.h>
const int SERVO_PIN = 5;
Servo servo;
#else
#include <Adafruit_PWMServoDriver.h>
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
#define SERVOMIN  150 // this is the 'minimum' pulse length count (out of 4096)
#define SERVOMAX  574 // this is the 'maximum' pulse length count (out of 4096)
#endif

/////////////////
// Util functions
/////////////////
void send_simple_message(const char* address)
{
  OSCMessage msg(address);
  send_message(msg);
}

void send_message(OSCMessage &msg)
{
  UdpOSC.beginPacket(outIp, outPort);
  msg.send(UdpOSC);
  UdpOSC.endPacket();
  msg.empty();
}

////////
// SETUP
////////
void setup(void)
{
  Serial.begin(115200);
  Serial.println("---");

  // set up wifi
  gotIpEventHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP & event)
  {
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // open ports
    UdpData.begin(dataPort);
    UdpOSC.begin(oscPort);

    // send handshake message
    UdpOSC.beginPacket(outIp, outPort);
    handshake_message().send(UdpOSC);
    UdpOSC.endPacket();

    Serial.println("broadcasting addresses:");
    Serial.println(broadcastIP1.toString());
    Serial.println(broadcastIP2.toString());
    Serial.println(broadcastIP3.toString());
    Serial.println(broadcastIP4.toString());

    connectedToWiFi = true;
  });

  disconnectedEventHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected & event)
  {
    connectedToWiFi = false;
    wifi_connect();
  });

  wifi_connect();
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }

  // set up servo
#ifndef SOLIST
  pinMode(SERVO_PIN, OUTPUT);
  servo.attach(SERVO_PIN);
  servo.write(SERVO_INIT_VALUE);
#else
  pwm.begin();
  pwm.setPWMFreq(60);  // Analog servos run at ~60 Hz updates
#endif

  // read target ip from EEPROM
  EEPROM.begin(512);
  outIp[0] = EEPROM.read(0);
  outIp[1] = EEPROM.read(1);
  outIp[2] = EEPROM.read(2);
  outIp[3] = EEPROM.read(3);

  Serial.print("target ip: ");
  Serial.println(outIp.toString());
}

void wifi_connect()
{
  Serial.print("Lost WiFi connection, reconnecting to ");
  Serial.print(ssid);
  Serial.println("...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
}

void set_remote_ip(const IPAddress& ip)
{
  // set remote IP
  outIp = IPAddress(ip);
  Serial.println(ip.toString());
  // TODO : deal with ipv6 or MAC addresses
  // store ip address
  EEPROM.write(0, (byte)outIp[0]);
  EEPROM.write(1, (byte)outIp[1]);
  EEPROM.write(2, (byte)outIp[2]);
  EEPROM.write(3, (byte)outIp[3]);
  EEPROM.commit();

  Serial.print("target ip is now: ");
  Serial.println(outIp.toString());
  gotHandshake = true;
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case WIFI_EVENT_STAMODE_GOT_IP:
      break;

    case WIFI_EVENT_STAMODE_DISCONNECTED:
      break;
  }
}

OSCMessage handshake_message()
{
  OSCMessage msg("/handshake");
  msg.add(SKULL_ID);
  msg.add(SKULL_NAME.c_str());
  msg.add(WiFi.localIP().toString().c_str());

  // get and send target IP
  msg.add(outIp.toString().c_str());
  return msg;
}

void loop(void)
{
  if (!connectedToWiFi) return;
  // send ping
  if ((unsigned long)(millis() - lastPingTime) > pingTimeInMs)
  {
    if (gotHandshake)
    {
      OSCMessage pingmsg("/ping");
      pingmsg.add(SKULL_ID);
      pingmsg.add(SKULL_NAME.c_str());
      pingmsg.add(WiFi.localIP().toString().c_str());
      send_message(pingmsg);

      Serial.print("ping to: ");
      Serial.println(outIp.toString());
      if (treatedPackets) Serial.println(treatedPackets);
      treatedPackets = 0;
    } else
    {
      // broadcast handshake message
      UdpOSC.beginPacket(broadcastIP1, outPort);
      handshake_message().send(UdpOSC);
      UdpOSC.endPacket();
      UdpOSC.beginPacket(broadcastIP2, outPort);
      handshake_message().send(UdpOSC);
      UdpOSC.endPacket();
      UdpOSC.beginPacket(broadcastIP3, outPort);
      handshake_message().send(UdpOSC);
      UdpOSC.endPacket();
      UdpOSC.beginPacket(broadcastIP4, outPort);
      handshake_message().send(UdpOSC);
      UdpOSC.endPacket();
      UdpOSC.beginPacket(broadcastIP5, outPort);
      handshake_message().send(UdpOSC);
      UdpOSC.endPacket();
      Serial.println("--- broadcasting handshake... ---");
      delay(500);
    }
    lastPingTime = millis();
  }

  int packetSize;

  // Read raw data
  while ( (packetSize = UdpData.parsePacket()) > 0)
  {
    //Serial.printf("Received %d bytes from %s, port %d\n", packetSize, UdpData.remoteIP().toString().c_str(), UdpData.remotePort());
    int n = UdpData.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
    packetBuffer[n] = 0;
    int value = atoi(packetBuffer);
    //Serial.printf("UDP packet contents: %s\n", packetBuffer);

    // TODO: prendre en compte les moteurs de Jack ?
    OSCMessage servomsg("/servo");
    servomsg.add(value);
    set_servo(servomsg, 0);

    treatedPackets++;
    if (!gotHandshake)
      set_remote_ip(UdpData.remoteIP());
  }

  // route OSC messages
  OSCMessage bundleIN;
  if ( (packetSize = UdpOSC.parsePacket()) > 0)
  {
    while (packetSize--)
      bundleIN.fill(UdpOSC.read());

    if (!bundleIN.hasError())
    {
      treatedPackets++;
      if (!gotHandshake)
        set_remote_ip(UdpOSC.remoteIP());

      bundleIN.route("/handshake", get_handshake);
      bundleIN.route("/config", send_config);
      bundleIN.route("/servo", set_servo);
    } else {
      OSCErrorCode error = bundleIN.getError();
      Serial.print("error: ");
      Serial.println(error);
    }
  }
}

void get_handshake(OSCMessage & msg, int addrOffset)
{
  Serial.println("got handhake.");
  // exchange IP addresses
  if (msg.isInt(0) && msg.isInt(1) && msg.isInt(2) && msg.isInt(3))
    set_remote_ip(IPAddress(msg.getInt(0), msg.getInt(1), msg.getInt(2), msg.getInt(3)));
  else
    Serial.println("error: ip from handshake is not valid!");

  // TODO: useless, get remoteIP instead
}


void send_config(OSCMessage & msg, int addrOffset)
{
  OSCMessage m = handshake_message();
  send_message(m);
}


void set_servo(OSCMessage & msg, int addrOffset)
{
  //Serial.print("set servo ");
#ifndef SOLIST
  if (msg.isInt(0))
  {
    int servoValue = constrain(SERVO_INIT_VALUE + msg.getInt(0), 0, 180);
    servo.write(servoValue);
    //Serial.println(servoValue);
  }
  else send_simple_message("/error");
#else
  int index = -1;
  if (msg.match("/0", addrOffset)) index = 0;
  else if (msg.match("/1", addrOffset)) index = 1;
  else if (msg.match("/2", addrOffset)) index = 2;
  //if (msg.match("/3", addrOffset)) index = 3;
  else if (msg.match("/4", addrOffset)) index = 4;
  else if (msg.match("/5", addrOffset)) index = 5;
  else if (msg.match("/6", addrOffset)) index = 6;
  else index = 3;

  if (msg.isInt(0))
  {
    int servoValue = constrain(SERVO_INIT_VALUE + msg.getInt(0), 0, 180);
    pwm.setPWM(index, 0, map(servoValue, 0, 180, SERVOMIN, SERVOMAX));
    Serial.print(index);
    Serial.print(" - ");
    Serial.println(servoValue);
  }
  else send_simple_message("/error");

#endif
}
