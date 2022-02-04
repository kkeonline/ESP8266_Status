/* 
 * ESP8266 template
 * using: WIFIclient, NTPclient, OTAupdate, WEBserver, LEDblink, LINEnotify
 * Github: KKEonline
*/
 
#include <TimeLib.h>
#include <NTPClient.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <TridentTD_LineNotify.h>

#define delayTime 50 // Loop delay
#define wifiTimeout 30000
#define bright 128 // led pwm brightness 0=On 255=Off
#define LINE_TOKEN  "xxxxx"

const char* appVersion = "1.0";
const char* ssid1 = "SSID";
const char* password1 = "PASSWD";

const char* OTAname = "ESP8266-01";
const char* OTApasswd = "123456";

static const char ntpServerName[] = "th.pool.ntp.org";
const int timeZone = 7; // Bangkok Time

unsigned long lastMillis;          // will store last time LED was updated
const int intervalSlow = 1000; // Wifi connected blink rate
const int intervalFast =  200;  // Wifi disconnected blink rate
int MyInterval;           // current LED blink rate (milliseconds)

const int ledPin =  LED_BUILTIN;  // LED pin
int ledState; // current LED state

const String htmlHead = "<html>\
  <head>\
    <title>ESP8266 Status</title>\
    <style>\
      body { background-color: #ffffff; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
  ";

const String htmlFoot = "\
  </body>\
</html>";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServerName, timeZone*3600);

ESP8266WebServer server(80);

void Blink(){
  if(ledState){
    analogWrite(ledPin, bright);
  }else{
    digitalWrite(ledPin, HIGH);
  }
  ledState=!ledState;
}

int wifiConnected() {
  return (WiFi.status() == WL_CONNECTED);
}

time_t getNtpTime(){
   return timeClient.getEpochTime();
}

String digits(int i) {
    if(i < 10) {
       return("0" + String(i));
    } else {
       return(String(i));
    }
}

String dateString(time_t t) {
    return ( digits(day(t)) +"/"+ digits(month(t)) +"/"+ digits(year(t)) +"  "+ digits(hour(t)) +":"+ digits(minute(t)) +":"+ digits(second(t)));
}

String uptime() {
  String res="";
  unsigned long mil;
  unsigned long umil;
  int uday;
  int uhour;
  int umin;
  int usec;

  umil = millis();
  mil = (umil / 1000);
  usec= mil % 60;
  mil /= 60; // now it is minutes
  umin = mil % 60;
  mil /= 60; // now it is hours
  uhour = mil % 24;
  mil /= 24; // now it is days
  uday = mil;

  res += String(uday);
  res += " days ";
  res += String(uhour);
  res += " hours ";
  res += String(umin);
  res += " minutes ";
  res += String(usec);
  res += " seconds ";
 
  res += " (";
  res += String(umil);
  res += " millis)";

  return(res);
}

void handleRoot() {
  Serial.print(timeClient.getFormattedTime()+" HTTP Response request ... ");
  server.send(200, "text/plain", "OK");
  Serial.println("END");
}

void handleNotFound() {
  Serial.print(timeClient.getFormattedTime()+" HTTP Response request ... ");
  String message = "Not Found!\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  Serial.println("END");
}

void handleReset() {
  Serial.print(timeClient.getFormattedTime()+" HTTP Response request ... ");
  server.send(200, "text/plain", "Reset ESP");
  Serial.println("END");
  delay(1000);
  ESP.restart();
}

void handleStatus() {
  Serial.print(timeClient.getFormattedTime()+" HTTP Response request ... ");
  String message = htmlHead;
  message += "<h3>ESP Status Ver." + String(appVersion) + "</h3>";
  message += "<div>Local DateTime: ";
  message += dateString(now());
  message += "</div><div>Uptime: ";
  message += uptime();
  message += "</div><div>LED Blink rate: ";
  message += String(MyInterval);
  message += "</div><br>";
  message += htmlFoot;
  server.send(200, "text/html", message);
  Serial.println("END");
}

void setupOTA() {
  ArduinoOTA.setHostname(OTAname);
  ArduinoOTA.setPassword(OTApasswd);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
  });
  ArduinoOTA.begin();
}

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  ledState=LOW;

  WiFi.hostname(OTAname);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid1, password1);

  Serial.print("Connecting to wifi ");
  unsigned long Millis = millis();
  while (!wifiConnected()) { // Wait for the Wi-Fi to connect
    if (millis() - Millis >= wifiTimeout) {
      Serial.println(" timeout!");
      break;  
    }
    delay(intervalFast);
    Blink();
  }

  if(wifiConnected()) {
    Serial.print(timeClient.getFormattedTime()+" IP address: ");
    Serial.println(WiFi.localIP());
  }
  
  setupOTA();
  timeClient.begin();
  timeClient.update();
  delay(200);
  setSyncProvider(getNtpTime);
  setSyncInterval(30);
  LINE.setToken(LINE_TOKEN);

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/reset", handleReset);
  server.onNotFound(handleNotFound);
  server.begin();

  lastMillis = millis();
  LINE.notify(timeClient.getFormattedTime()+"\r\nRunning IP: " + WiFi.localIP().toString());
}

void loop(){
    if (wifiConnected()) {
      MyInterval=intervalSlow;
      ArduinoOTA.handle();
      timeClient.update();
      server.handleClient();
    } else {
      MyInterval=intervalFast;
    }

   unsigned long currentMillis = millis();
   if (currentMillis - lastMillis >= MyInterval) {
        lastMillis = currentMillis;
        Blink();
    }

   // do something else
   // ...
   // ...

   delay(delayTime);
}
